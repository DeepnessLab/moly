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

package org.opendaylight.tutorial.tutorial_L2_forwarding.internal;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;

import org.opendaylight.controller.sal.action.Action;
import org.opendaylight.controller.sal.action.Output;
import org.opendaylight.controller.sal.core.ConstructionException;
import org.opendaylight.controller.sal.core.Node;
import org.opendaylight.controller.sal.core.NodeConnector;
import org.opendaylight.controller.sal.flowprogrammer.Flow;
import org.opendaylight.controller.sal.flowprogrammer.IFlowProgrammerService;
import org.opendaylight.controller.sal.match.Match;
import org.opendaylight.controller.sal.match.MatchField;
import org.opendaylight.controller.sal.match.MatchType;
import org.opendaylight.controller.sal.packet.BitBufferHelper;
import org.opendaylight.controller.sal.packet.Ethernet;
import org.opendaylight.controller.sal.packet.IDataPacketService;
import org.opendaylight.controller.sal.packet.IListenDataPacket;
import org.opendaylight.controller.sal.packet.Packet;
import org.opendaylight.controller.sal.packet.PacketResult;
import org.opendaylight.controller.sal.packet.RawPacket;
import org.opendaylight.controller.sal.utils.Status;
import org.opendaylight.controller.switchmanager.ISwitchManager;
import org.opendaylight.controller.switchmanager.Switch;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;
import org.osgi.framework.BundleException;
import org.osgi.framework.FrameworkUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class TutorialL2Forwarding implements IListenDataPacket {
	private static final Logger logger = LoggerFactory
			.getLogger(TutorialL2Forwarding.class);
	private ISwitchManager switchManager = null;
	private IFlowProgrammerService programmer = null;
	private IDataPacketService dataPacketService = null;
	private final Map<Node, Map<Long, NodeConnector>> mac_to_port_per_switch = new HashMap<Node, Map<Long, NodeConnector>>();
	private final String function = "switch";

	void setDataPacketService(IDataPacketService s) {
		this.dataPacketService = s;
	}

	void unsetDataPacketService(IDataPacketService s) {
		if (this.dataPacketService == s) {
			this.dataPacketService = null;
		}
	}

	public void setFlowProgrammerService(IFlowProgrammerService s) {
		this.programmer = s;
	}

	public void unsetFlowProgrammerService(IFlowProgrammerService s) {
		if (this.programmer == s) {
			this.programmer = null;
		}
	}

	void setSwitchManager(ISwitchManager s) {
		logger.debug("SwitchManager set");
		this.switchManager = s;
	}

	void unsetSwitchManager(ISwitchManager s) {
		if (this.switchManager == s) {
			logger.debug("SwitchManager removed!");
			this.switchManager = null;
		}
	}

	/**
	 * Function called by the dependency manager when all the required
	 * dependencies are satisfied
	 * 
	 */
	void init() {
		logger.info("Initialized");
		// Disabling the SimpleForwarding and ARPHandler bundle to not conflict
		// with this one
		BundleContext bundleContext = FrameworkUtil.getBundle(this.getClass())
				.getBundleContext();
		for (Bundle bundle : bundleContext.getBundles()) {
			if (bundle.getSymbolicName().contains("simpleforwarding")) {
				try {
					bundle.uninstall();
				} catch (BundleException e) {
					logger.error(
							"Exception in Bundle uninstall "
									+ bundle.getSymbolicName(), e);
				}
			}
		}

	}

	/**
	 * Function called by the dependency manager when at least one dependency
	 * become unsatisfied or when the component is shutting down because for
	 * example bundle is being stopped.
	 * 
	 */
	void destroy() {
	}

	/**
	 * Function called by dependency manager after "init ()" is called and after
	 * the services provided by the class are registered in the service registry
	 * 
	 */
	void start() {
		if (isRemoveAllFlows()) {
			logger.info("DELTETING ALL FLOWS!");
			List<Switch> switches = switchManager.getNetworkDevices();
			for (Switch switchNode : switches) {
				Status status = programmer.removeAllFlows(switchNode.getNode());
				if (!status.isSuccess()) {
					logger.error("error while removing flows: "
							+ status.getDescription());
				}
			}
		}
		logger.info("Started");
	}

	/**
	 * patch method to delete all flows when needed
	 * 
	 * @return
	 */
	private boolean isRemoveAllFlows() {
		InputStream inputStream;
		try {
			inputStream = new FileInputStream(new File(
					"configuration/config.ini"));
			Properties prop = new Properties();
			prop.load(inputStream);
			String property = prop.getProperty(
					"topologyForwarding.deleteAllFlow", "false");
			return property.equals("true");
		} catch (IOException e) {
			logger.warn("cant open config.ini");
			return false;
		}
	}

	/**
	 * Function called by the dependency manager before the services exported by
	 * the component are unregistered, this will be followed by a "destroy ()"
	 * calls
	 * 
	 */
	void stop() {
		logger.info("Stopped");
	}

	private void floodPacket(RawPacket inPkt) {
		NodeConnector incoming_connector = inPkt.getIncomingNodeConnector();
		Node incoming_node = incoming_connector.getNode();

		Set<NodeConnector> nodeConnectors = this.switchManager
				.getUpNodeConnectors(incoming_node);

		for (NodeConnector p : nodeConnectors) {
			if (!p.equals(incoming_connector)) {
				sendPacket(inPkt, p);
			}
		}
	}

	@Override
	public PacketResult receiveDataPacket(RawPacket inPkt) {
		if (inPkt == null) {
			return PacketResult.IGNORED;
		}
		logger.trace("Received a frame of size: {}",
				inPkt.getPacketData().length);

		Packet formattedPak = this.dataPacketService.decodeDataPacket(inPkt);
		NodeConnector incoming_connector = inPkt.getIncomingNodeConnector();
		Node incoming_node = incoming_connector.getNode();

		if (formattedPak instanceof Ethernet) {
			byte[] srcMAC = ((Ethernet) formattedPak).getSourceMACAddress();
			byte[] dstMAC = ((Ethernet) formattedPak)
					.getDestinationMACAddress();

			// Hub implementation
			if (function.equals("hub")) {
				floodPacket(inPkt);
				return PacketResult.CONSUME;
			}

			// Switch
			else {
				long srcMAC_val = BitBufferHelper.toNumber(srcMAC);
				long dstMAC_val = BitBufferHelper.toNumber(dstMAC);

				Match match = new Match();
				match.setField(new MatchField(MatchType.IN_PORT,
						incoming_connector));
				match.setField(new MatchField(MatchType.DL_DST, dstMAC.clone()));

				// Set up the mapping: switch -> src MAC address -> incoming
				// port
				if (this.mac_to_port_per_switch.get(incoming_node) == null) {
					this.mac_to_port_per_switch.put(incoming_node,
							new HashMap<Long, NodeConnector>());
				}
				this.mac_to_port_per_switch.get(incoming_node).put(srcMAC_val,
						incoming_connector);

				NodeConnector dst_connector = this.mac_to_port_per_switch.get(
						incoming_node).get(dstMAC_val);

				// Do I know the destination MAC?
				if (dst_connector != null) {

					List<Action> actions = new ArrayList<Action>();
					actions.add(new Output(dst_connector));

					Flow f = new Flow(match, actions);
					f.setPriority((short) 1);

					// Modify the flow on the network node
					Status status = programmer.addFlow(incoming_node, f);
					if (!status.isSuccess()) {
						logger.warn(
								"SDN Plugin failed to program the flow: {}. The failure is: {}",
								f, status.getDescription());
						return PacketResult.IGNORED;
					}
					logger.debug("Installed flow {} in node {}", f,
							incoming_node);
					sendPacket(inPkt, dst_connector);
					return PacketResult.CONSUME;
				} else
					floodPacket(inPkt);
			}
		}
		return PacketResult.IGNORED;
	}

	private void sendPacket(RawPacket inPkt, NodeConnector dst_connector) {
		RawPacket destPkt;
		try {
			destPkt = new RawPacket(inPkt);
			destPkt.setOutgoingNodeConnector(dst_connector);
			this.dataPacketService.transmitDataPacket(destPkt);
		} catch (ConstructionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}
}
