package org.opendaylight.dpi_tsa.internal;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import org.opendaylight.controller.hosttracker.IfIptoHost;
import org.opendaylight.controller.hosttracker.hostAware.HostNodeConnector;
import org.opendaylight.controller.sal.action.Action;
import org.opendaylight.controller.sal.action.PushVlan;
import org.opendaylight.controller.sal.core.NodeConnector;
import org.opendaylight.controller.sal.match.Match;
import org.opendaylight.controller.sal.match.MatchField;
import org.opendaylight.controller.sal.match.MatchType;
import org.opendaylight.controller.sal.utils.EtherTypes;
import org.opendaylight.controller.sal.utils.NetUtils;
import org.opendaylight.controller.switchmanager.Switch;
import org.openflow.protocol.OFMatch;
import org.openflow.protocol.OFOXMFieldType;
import org.openflow.protocol.OFVlanId;

public class FlowUtils {

	/**
	 * return match field representing packet with tag tag=0 means no tag
	 * 
	 * @param tag
	 * @return
	 */

	static List<MatchField> generateMatchOnTag(int tag) {
		LinkedList<MatchField> result = new LinkedList<MatchField>();
		if (tag == 0) {
			// TODO: make this work!!!
			result.add(new MatchField(MatchType.DL_VLAN, MatchType.DL_VLAN_NONE));
		} else {
			result.add(new MatchField(MatchType.DL_VLAN, (short) (tag)));
		}
		return result;
	}

	/**
	 * this method return list of HostNodeConnector corresponding to the input
	 * IP list the method uses the host tacker service received on the
	 * constructor
	 * 
	 * @param policyChain
	 * @param hostTarcker
	 *            TODO
	 * @return
	 */
	static List<HostNodeConnector> findHosts(List<InetAddress> policyChain,
			IfIptoHost hostTarcker) {
		List<HostNodeConnector> policyChainHosts = new ArrayList<HostNodeConnector>();
		for (InetAddress mbAddress : policyChain) {
			try {
				HostNodeConnector host;
				TSAGenerator.logger.info(String.format(
						"looking for host %s ..", mbAddress));
				host = hostTarcker.discoverHost(mbAddress).get();
				if (host != null) {
					TSAGenerator.logger.info(String.format("host %s found!",
							mbAddress));
					policyChainHosts.add(host);
				} else {
					TSAGenerator.logger.error(String.format(
							"host %s not found :(", mbAddress));
				}

			} catch (Exception e) {
				TSAGenerator.logger.error(String.format(
						"Problem occoured while looking for host %s : %s",
						mbAddress, e.getMessage()));

			}
		}
		return policyChainHosts;
	}

	/**
	 * return action that tags the packets
	 * 
	 * @param tag
	 * @return
	 */
	static Action generateTagAction(short tag) {
		return new PushVlan(EtherTypes.VLANTAGGED.intValue(), 0, 0, tag);
	}

	/**
	 * this method used to set multiple fields into a match object
	 * 
	 * @param match
	 *            the match to update
	 * @param matchFields
	 *            list of matchfields to set
	 */
	public static void setFields(Match match, List<MatchField> matchFields) {
		for (MatchField matchField : matchFields) {
			match.setField(matchField);
		}
	}

	public static List<MatchField> generateMatchOnConnector(
			NodeConnector nodeConnector) {
		return Arrays.asList(new MatchField(MatchType.IN_PORT, nodeConnector));
	}

	public static Set<NodeConnector> getSwitchHosts(Switch switchNode,
			IfIptoHost hostTracker) {
		Set<HostNodeConnector> hosts = hostTracker.getAllHosts();
		Set<NodeConnector> hostsNodeConnectors = new HashSet<>(
				getHostsConnectors(hosts));

		Set<NodeConnector> nodeConnectors = switchNode.getNodeConnectors();
		hostsNodeConnectors.retainAll(nodeConnectors);
		return hostsNodeConnectors;
	}

	public static Collection<NodeConnector> getHostsConnectors(
			Collection<HostNodeConnector> hosts) {
		Set<NodeConnector> hostsNodeConnectors = new HashSet<NodeConnector>();
		for (HostNodeConnector host : hosts)
			hostsNodeConnectors.add(host.getnodeConnector());
		return hostsNodeConnectors;
	}

	/**
	 * return Match object representing the traffic that should traverse the TSA
	 * currently ICMP hard-coded
	 * 
	 * @param trafficClass
	 * 
	 * @return
	 */
	static Match parseMatch(String trafficClass) {
		OFMatch match = OFMatch.fromString(trafficClass);
		Match result = convertOFMatch(match);
		return result;
	}

	private static Match convertOFMatch(OFMatch ofMatch) {

		Match salMatch = new Match();

		if (ofMatch.getDataLayerSource() != null
				&& !NetUtils.isZeroMAC(ofMatch.getDataLayerSource())) {
			byte srcMac[] = ofMatch.getDataLayerSource();
			salMatch.setField(new MatchField(MatchType.DL_SRC, srcMac.clone()));
		}
		if (ofMatch.getDataLayerDestination() != null
				&& !NetUtils.isZeroMAC(ofMatch.getDataLayerDestination())) {
			byte dstMac[] = ofMatch.getDataLayerDestination();
			salMatch.setField(new MatchField(MatchType.DL_DST, dstMac.clone()));
		}
		if (ofMatch.fieldExists(OFOXMFieldType.ETH_TYPE)) {
			salMatch.setField(new MatchField(MatchType.DL_TYPE, ofMatch
					.getDataLayerType()));
		}
		short vlan = ofMatch.getDataLayerVirtualLan();
		if (vlan != OFVlanId.OFPVID_NONE.getValue()) {
			salMatch.setField(new MatchField(MatchType.DL_VLAN, vlan));
		}
		if (ofMatch.fieldExists(OFOXMFieldType.VLAN_PCP)) {
			salMatch.setField(MatchType.DL_VLAN_PR,
					ofMatch.getDataLayerVirtualLanPriorityCodePoint());
		}
		if (ofMatch.getNetworkSource() != 0) {
			salMatch.setField(MatchType.NW_SRC,
					NetUtils.getInetAddress(ofMatch.getNetworkSource()),
					NetUtils.getInetAddress(ofMatch.getNetworkSourceMask()));
		}
		if (ofMatch.getNetworkDestination() != 0) {
			salMatch.setField(MatchType.NW_DST, NetUtils.getInetAddress(ofMatch
					.getNetworkDestination()), NetUtils.getInetAddress(ofMatch
					.getNetworkDestinationMask()));
		}
		if (ofMatch.fieldExists(OFOXMFieldType.IP_DSCP)) {
			salMatch.setField(MatchType.NW_TOS,
					NetUtils.getUnsignedByte(ofMatch.getNetworkTypeOfService()));
		}
		if (ofMatch.fieldExists(OFOXMFieldType.IP_PROTO)) {
			salMatch.setField(MatchType.NW_PROTO, ofMatch.getNetworkProtocol());
		}
		if (ofMatch.fieldExists(OFOXMFieldType.TCP_SRC)
				|| ofMatch.fieldExists(OFOXMFieldType.UDP_SRC)
				|| ofMatch.fieldExists(OFOXMFieldType.SCTP_SRC)) {
			salMatch.setField(MatchType.TP_SRC, ofMatch.getTransportSource());
		}
		if (ofMatch.fieldExists(OFOXMFieldType.TCP_DST)
				|| ofMatch.fieldExists(OFOXMFieldType.UDP_DST)
				|| ofMatch.fieldExists(OFOXMFieldType.SCTP_DST)) {
			salMatch.setField(MatchType.TP_DST,
					ofMatch.getTransportDestination());
		}
		return salMatch;
	}
}
