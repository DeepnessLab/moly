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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Dictionary;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.concurrent.ExecutionException;

import org.opendaylight.controller.hosttracker.IfNewHostNotify;
import org.opendaylight.controller.hosttracker.IfIptoHost;
import org.opendaylight.controller.hosttracker.hostAware.HostNodeConnector;
import org.opendaylight.controller.sal.action.Action;
import org.opendaylight.controller.sal.action.Output;
import org.opendaylight.controller.sal.core.ConstructionException;
import org.opendaylight.controller.sal.core.Edge;
import org.opendaylight.controller.sal.core.Node;
import org.opendaylight.controller.sal.core.NodeConnector;
import org.opendaylight.controller.sal.core.Path;
import org.opendaylight.controller.sal.flowprogrammer.Flow;
import org.opendaylight.controller.sal.flowprogrammer.IFlowProgrammerService;
import org.opendaylight.controller.sal.match.Match;
import org.opendaylight.controller.sal.match.MatchType;
import org.opendaylight.controller.sal.packet.Ethernet;
import org.opendaylight.controller.sal.packet.IDataPacketService;
import org.opendaylight.controller.sal.packet.IListenDataPacket;
import org.opendaylight.controller.sal.packet.IPv4;
import org.opendaylight.controller.sal.packet.Packet;
import org.opendaylight.controller.sal.packet.PacketResult;
import org.opendaylight.controller.sal.packet.RawPacket;
import org.opendaylight.controller.sal.routing.IRouting;
import org.opendaylight.controller.sal.utils.Status;
import org.opendaylight.controller.switchmanager.ISwitchManager;
import org.opendaylight.controller.switchmanager.Switch;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class TopologyForwarding implements IListenDataPacket, IfNewHostNotify {
	private static final Logger logger = LoggerFactory
			.getLogger(TopologyForwarding.class);
	private ISwitchManager switchManager = null;
	private IFlowProgrammerService programmer = null;
	private IDataPacketService dataPacketService = null;
	private IfIptoHost hostTracker;
	private IRouting routing;
	private HashMap<Flow, Node> _allFlows;
	private HashMap<HostNodeConnector, List<Flow>> _hostFlows;

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

	void setHostTracker(IfIptoHost s) {
		this.hostTracker = s;
	}

	void setRoutingService(IRouting s) {
		this.routing = s;
	}

	void unsetRoutingService(IRouting s) {
		if (this.routing == s) {
			this.routing = null;
		}
	}

	void unsetHostTracker(IfIptoHost s) {
		if (this.hostTracker == s) {
			this.hostTracker = null;
		}
	}

	/**
	 * Function called by the dependency manager when all the required
	 * dependencies are satisfied
	 * 
	 */
	void init() {
		logger.info("Initialized");

	}

	/**
	 * Function called by the dependency manager when at least one dependency
	 * become unsatisfied or when the component is shutting down because for
	 * example bundle is being stopped.
	 * 
	 */
	void destroy() {
		logger.info("deleting forwarding flows");
		for (Flow flow : _allFlows.keySet()) {
			removeFlow(flow);

		}
	}

	private void removeFlow(Flow flow) {
		Status status = programmer.removeFlow(_allFlows.get(flow), flow);
		if (!status.isSuccess())
			logger.error("error while removing flow: "
					+ status.getDescription());
		_allFlows.remove(flow);
	}

	/**
	 * Function called by dependency manager after "init ()" is called and after
	 * the services provided by the class are registered in the service registry
	 * 
	 */
	void start() {
		_allFlows = new HashMap<>();
		_hostFlows = new HashMap<>();
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

	static private Inet4Address intToInetAddress(int i) {
		byte b[] = new byte[] { (byte) ((i >> 24) & 0xff),
				(byte) ((i >> 16) & 0xff), (byte) ((i >> 8) & 0xff),
				(byte) (i & 0xff) };
		Inet4Address addr;
		try {
			addr = (Inet4Address) InetAddress.getByAddress(b);
		} catch (UnknownHostException e) {
			return null;
		}

		return addr;
	}

	/**
	 * returns actions that forward the packets arriving into the sw to the
	 * nextHost this method this action should be set to the switch attached to
	 * the host uses the routing service from constructor
	 * 
	 * 
	 * @param host
	 * @param nextHost
	 * @return
	 */
	private NodeConnector outputToDst(Node sw, HostNodeConnector nextHost) {
		NodeConnector result = null;
		Path route = routing.getRoute(sw, nextHost.getnodeconnectorNode());
		if (route == null) { // destination host is attached to switch
			result = nextHost.getnodeConnector();
		} else { // forward to destination host
			result = route.getEdges().get(0).getTailNodeConnector();
		}
		return result;
	}

	private Flow generateRouteToHostFlow(HostNodeConnector host,
			Node incoming_node) {
		short priority = 1;
		List<Action> actions = new ArrayList<Action>();
		Action output = new Output(outputToDst(incoming_node, host));
		actions.add(output);
		Match match = new Match();
		match.setField(MatchType.DL_TYPE, (short) 0x0800);
		match.setField(MatchType.NW_DST, host.getNetworkAddress());
		Flow flow = new Flow(match, actions);
		flow.setPriority(priority);
		return flow;
	}

	private void floodPacket(RawPacket inPkt) {
		NodeConnector incoming_connector = inPkt.getIncomingNodeConnector();
		Node incoming_node = incoming_connector.getNode();

		Set<NodeConnector> nodeConnectors = this.switchManager
				.getUpNodeConnectors(incoming_node);

		for (NodeConnector p : nodeConnectors) {
			if (!p.equals(incoming_connector)) {
				try {
					RawPacket destPkt = new RawPacket(inPkt);
					destPkt.setOutgoingNodeConnector(p);
					this.dataPacketService.transmitDataPacket(destPkt);
				} catch (ConstructionException e2) {
					continue;
				}
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

		if (!(formattedPak instanceof Ethernet)) {
			return PacketResult.IGNORED;
		}
		Ethernet l2Pkt = (Ethernet) formattedPak;
		Packet l3Pkt = l2Pkt.getPayload();
		if (!(l3Pkt instanceof IPv4)) {
			floodPacket(inPkt);
			return PacketResult.IGNORED;
		}
		IPv4 ipPacket = (IPv4) l3Pkt;
		Inet4Address ipDest = intToInetAddress(ipPacket.getDestinationAddress());
		logger.debug("got ip packet to: " + ipDest);
		HostNodeConnector dstHost;
		try {
			dstHost = hostTracker.discoverHost(ipDest).get();

			if (dstHost == null) {
				floodPacket(inPkt);
				return PacketResult.KEEP_PROCESSING;
			}

			Flow flow = generateRouteToHostFlow(dstHost, incoming_node);
			if (!programFlow(incoming_node, dstHost, flow))
				return PacketResult.IGNORED;
			sendPacket(inPkt, outputToDst(incoming_node, dstHost));

			return PacketResult.CONSUME;
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (ExecutionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return PacketResult.IGNORED;
	}

	private boolean programFlow(Node incoming_node, HostNodeConnector dstHost,
			Flow flow) {
		Status status = programmer.addFlow(incoming_node, flow);
		if (!status.isSuccess()) {
			logger.warn(
					"SDN Plugin failed to program the flow: {}. The failure is: {}",
					flow, status.getDescription());
			return false;
		}
		logger.info("Installed flow {} in node {}", flow, incoming_node);
		_allFlows.put(flow, incoming_node);
		if (!_hostFlows.containsKey(dstHost))
			_hostFlows.put(dstHost, new LinkedList<Flow>());
		_hostFlows.get(dstHost).add(flow);
		return true;
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

	@Override
	public void notifyHTClient(HostNodeConnector host) {
		logger.info("new host added: " + host);

	}

	@Override
	public void notifyHTClientHostRemoved(HostNodeConnector host) {
		logger.info("host removed: " + host.getNetworkAddressAsString());
		removeHostFlows(host);
	}

	private void removeHostFlows(HostNodeConnector host) {
		if (!_hostFlows.containsKey(host)) {
			logger.warn("removed host has no flows, shouldnt happen");
			return;
		}
		logger.info("removing host flows: " + host.getNetworkAddressAsString());
		for (Flow flow : _hostFlows.get(host)) {
			removeFlow(flow);
		}
		_hostFlows.remove(host);
	}

}
