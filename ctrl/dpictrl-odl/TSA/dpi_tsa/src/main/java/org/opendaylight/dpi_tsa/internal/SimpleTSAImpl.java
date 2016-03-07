/*
 * Copyright (C) 2014 SDN Hub

 Licensed under the GNU GENERAL PUBLIC LICENSE, Version 3.
 You may not use this file except in compliance with this License.
 You may obtain a copy of the License at

    http://www.gnu.org/licenses/gpl-3.0.txt

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 implied.

 *
 */

package org.opendaylight.dpi_tsa.internal;

import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.opendaylight.controller.hosttracker.IfIptoHost;
import org.opendaylight.controller.sal.action.Action;
import org.opendaylight.controller.sal.action.Controller;
import org.opendaylight.controller.sal.core.Node;
import org.opendaylight.controller.sal.flowprogrammer.Flow;
import org.opendaylight.controller.sal.flowprogrammer.IFlowProgrammerService;
import org.opendaylight.controller.sal.match.Match;
import org.opendaylight.controller.sal.match.MatchType;
import org.opendaylight.controller.sal.packet.IDataPacketService;
import org.opendaylight.controller.sal.packet.PacketResult;
import org.opendaylight.controller.sal.routing.IRouting;
import org.opendaylight.controller.sal.utils.EtherTypes;
import org.opendaylight.controller.sal.utils.Status;
import org.opendaylight.controller.switchmanager.ISwitchManager;
import org.opendaylight.controller.switchmanager.Switch;
import org.opendaylight.controller.topologymanager.ITopologyManager;
import org.opendaylight.dpi_tsa.ConfigChangedDelegation;
import org.opendaylight.dpi_tsa.ConfigurationManager;
import org.opendaylight.dpi_tsa.ITrafficSteeringService;
import org.opendaylight.dpi_tsa.listener.TSAListener;
import org.opendaylight.dpi_tsa.listener.TsaSocketListener;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import Common.Protocol.RawPolicyChain;

public class SimpleTSAImpl implements ITrafficSteeringService,
		ConfigChangedDelegation {
	private static final Logger logger = LoggerFactory
			.getLogger(SimpleTSAImpl.class);
	private static final short ARP_PRIORITY = 8;
	private ISwitchManager switchManager = null;
	private IFlowProgrammerService programmer = null;
	private IDataPacketService dataPacketService = null;
	private ITopologyManager topologyManager = null;
	private IRouting routing = null;
	private IfIptoHost hostTracker = null;
	private List<RawPolicyChain> _currentPolicyChain;
	private List<Map<Node, List<Flow>>> _flows = null;
	private Map<String, Match> _trafficClasses;
	private TSAListener _listener;
	private ConfigurationManager _config;
	private TSAGenerator _tsaGenerator;

	/**
	 * Function called by dependency manager after "init ()" is called and after
	 * the services provided by the class are registered in the service registry
	 * 
	 */
	void start() {
		logger.info("Started");
		_flows = new LinkedList<Map<Node, List<Flow>>>();
		_trafficClasses = new HashMap<String, Match>();
		_config = ConfigurationManager.getInstance();
		_config.setPolicyChangedDelegation(this);
		List<RawPolicyChain> configPolicyChains = getConfigPolicyChains();
		_tsaGenerator = new TSAGenerator(routing, hostTracker, switchManager);
		// setArpFlow();
		if (configPolicyChains != null)
			applyPolicyChain(configPolicyChains);
		else
			logger.warn("no initial tsa policy");
		_listener.start();
	}

	/*
	 * this method set flow to send arp messages to controller with high
	 * priority, this is done in order to keep the host tracker working with any
	 * forwarding bundle
	 */
	private void setArpFlow() {
		Match match = new Match();
		match.setField(MatchType.DL_TYPE, EtherTypes.ARP.shortValue());
		Action action = new Controller();
		Flow flow = new Flow(match, Arrays.asList(action));
		flow.setPriority(ARP_PRIORITY);
		List<Switch> switches = switchManager.getNetworkDevices();
		for (Switch switchI : switches) {
			programmer.addFlow(switchI.getNode(), flow);
		}
		logger.info("arp flows added!");
	}

	private List<RawPolicyChain> getConfigPolicyChains() {

		List<RawPolicyChain> policyChainsConfig = _config
				.getPolicyChainsConfig();
		return policyChainsConfig;

	}

	/**
	 * apply the received policy chain on the network
	 * 
	 * @param policyChain
	 *            ordered array of hosts in the network each packet should
	 *            traverse (currently only ICMP)
	 */
	@Override
	public void applyPolicyChain(List<RawPolicyChain> policyChains) {
		if (policyChains.equals(_currentPolicyChain)) {
			logger.warn("policy chain already exists: " + policyChains);
			return;
		}
		if (policyChains.size() == 0) {
			logger.warn("got empty policy chain - clean rules ");
			removeFlows(_flows);
			_flows = null;
			_currentPolicyChain = policyChains;
			return;
		}
		logger.info("applying chain: " + policyChains);
		removeFlows(_flows);
		for (RawPolicyChain policyChain : policyChains) {
			Match trafficMatch = FlowUtils.parseMatch(policyChain.trafficClass);
			Map<Node, List<Flow>> chainsFlows = _tsaGenerator.generateRules(
					policyChain.chain, trafficMatch);
			programFlows(chainsFlows);
			_flows.add(chainsFlows);

		}

		_currentPolicyChain = policyChains;
	}

	private void removeFlows(List<Map<Node, List<Flow>>> flowsList) {
		logger.info("remove old rules");
		for (Map<Node, List<Flow>> flows : flowsList) {
			for (Node node : flows.keySet()) {
				for (Flow flow : flows.get(node)) {
					programmer.removeFlow(node, flow);
				}
			}
		}
		_flows.clear();

	}

	private void programFlows(Map<Node, List<Flow>> flows) {
		for (Node node : flows.keySet()) {
			for (Flow flow : flows.get(node)) {
				Status status = programmer.addFlow(node, flow);

				if (status.isSuccess()) {
					logger.debug(String.format("install flow %s to node %s",
							flow, node));
				} else {
					logger.error(String.format(
							"error while adding flow %s to node %s : %s", flow,
							node, status.getDescription()));
				}
			}
		}
	}

	/**
	 * Function called by the dependency manager before the services exported by
	 * the component are unregistered, this will be followed by a "destroy ()"
	 * calls
	 * 
	 */
	void stop() {
		_listener.stop();
		logger.info("Stopped");
	}

	/**
	 * Function called by the dependency manager when all the required
	 * dependencies are satisfied
	 * 
	 */
	void init() {
		_listener = new TsaSocketListener();
		_listener.setTSA(this);
		_listener.init();
		logger.info("Initialized");
	}

	/**
	 * Function called by the dependency manager when at least one dependency
	 * become unsatisfied or when the component is shutting down because for
	 * example bundle is being stopped.
	 * 
	 */
	void destroy() {
		removeFlows(_flows);
	}

	void setHostTracker(IfIptoHost s) {
		this.hostTracker = s;
	}

	void unsetHostTracker(IfIptoHost s) {
		if (this.hostTracker == s) {
			this.hostTracker = null;
		}
	}

	void setDataPacketService(IDataPacketService s) {
		this.dataPacketService = s;
	}

	void unsetDataPacketService(IDataPacketService s) {
		if (this.dataPacketService == s) {
			this.dataPacketService = null;
		}
	}

	void setRoutingService(IRouting s) {
		this.routing = s;
	}

	void unsetRoutingService(IRouting s) {
		if (this.routing == s) {
			this.routing = null;
		}
	}

	public void setFlowProgrammerService(IFlowProgrammerService s) {
		this.programmer = s;
	}

	public void unsetFlowProgrammerService(IFlowProgrammerService s) {
		if (this.programmer == s) {
			logger.error("FlowProgrammer removed!");
			this.programmer = null;
		}
	}

	public void unsetTopologyManager(ITopologyManager s) {
		if (this.topologyManager == s) {
			this.topologyManager = null;
		}
	}

	public void setTopologyManager(ITopologyManager s) {
		this.topologyManager = s;
	}

	void setSwitchManager(ISwitchManager s) {
		logger.debug("SwitchManager set");
		this.switchManager = s;
	}

	void unsetSwitchManager(ISwitchManager s) {
		if (this.switchManager == s) {
			logger.error("SwitchManager removed!");
			this.switchManager = null;
		}
	}

	@Override
	public List<RawPolicyChain> getPolicyChains() {
		return _currentPolicyChain;
	}

	@Override
	public void onConfigurationChange(ConfigurationManager configurationManager) {
		logger.info("Configuration policy chains changed - updating TSA!");
		applyPolicyChain(getConfigPolicyChains());
		_listener.sendPolicyChains();

	}

}
