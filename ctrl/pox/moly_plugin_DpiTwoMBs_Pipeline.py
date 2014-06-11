""" 
This is a simple plug in file for running MoLy with POX.
It assumes that the sample topology is used.
This file is based on the original OpenFlow tutorial.
"""

from pox.core import core
import pox.openflow.libopenflow_01 as of

log = core.getLogger()

ETHERTYPE_IPV4 = 0x0800
ETHERTYPE_ARP = 0x0806

IPPROTO_ICMP = 0x01
IPPROTO_TCP = 0x06
IPPROTO_UDP = 0x11

DSCP_BEST_EFF = 0
DSCP_IMM_1 = 16
DSCP_IMM_2 = 20

class Tutorial (object):

    def __init__ (self, connection):
        self.connection = connection
        connection.addListeners(self)
        self.moly_set_entries()

    def send_flow_mod(self, in_port=None, dl_type=None, dl_src=None, dl_dst=None, nw_proto=None, dl_vlan=None, nw_tos=None, actions=[], buffer_id=None, raw_data=None):
        fm = of.ofp_flow_mod()
        if in_port is not None:
            fm.match.in_port = in_port
        if dl_type is not None:
            fm.match.dl_type = dl_type
        if dl_src is not None:
            fm.match.dl_src = dl_src
        if dl_dst is not None:
            fm.match.dl_dst = dl_dst
        if nw_proto is not None:
            fm.match.nw_proto = nw_proto
        if dl_vlan is not None:
            fm.match.dl_vlan = dl_vlan
        if nw_tos is not None:
            fm.match.nw_tos = nw_tos

        # it is not mandatory to set fm.data or fm.buffer_id
        if buffer_id != -1 and buffer_id is not None:
            fm.buffer_id = buffer_id 
        else:
            if raw_data is not None:
                fm.data = raw_data

        fm.actions = actions

        # Send message to switch
        self.connection.send(fm)

    def moly_set_entries(self):
        if self.connection.dpid == 1:
            # Set ARP forwarding
            self.send_flow_mod(in_port=1, dl_type=ETHERTYPE_ARP, actions=[of.ofp_action_output(port=4)])
            self.send_flow_mod(in_port=4, dl_type=ETHERTYPE_ARP, actions=[of.ofp_action_output(port=1)])

            # Set ICMP forwarding
            self.send_flow_mod(in_port=1, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_ICMP, actions=[of.ofp_action_output(port=4)])
            self.send_flow_mod(in_port=4, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_ICMP, actions=[of.ofp_action_output(port=1)])

            # Set TCP/UDP forwarding
            # h1 to DPI
            self.send_flow_mod(in_port=1, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_TCP, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_IMM_1), of.ofp_action_output(port=2)])
            self.send_flow_mod(in_port=1, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_UDP, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_IMM_1), of.ofp_action_output(port=2)])
            # h2 to DPI
            self.send_flow_mod(in_port=4, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_TCP, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_IMM_2), of.ofp_action_output(port=2)])
            self.send_flow_mod(in_port=4, dl_type=ETHERTYPE_IPV4, nw_proto=IPPROTO_UDP, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_IMM_2), of.ofp_action_output(port=2)])
            # DPI to MBOX1
            self.send_flow_mod(in_port=2, dl_type=ETHERTYPE_IPV4, actions=[of.ofp_action_output(port=3)])
            # MBOX1 to MBOX2
            self.send_flow_mod(in_port=3, dl_type=ETHERTYPE_IPV4, actions=[of.ofp_action_output(port=5)])
            # MBOX2 to h2
            self.send_flow_mod(in_port=5, dl_type=ETHERTYPE_IPV4, nw_tos=DSCP_IMM_1, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_BEST_EFF), of.ofp_action_output(port=4)])
            # MBOX2 to h1
            self.send_flow_mod(in_port=5, dl_type=ETHERTYPE_IPV4, nw_tos=DSCP_IMM_2, actions=[of.ofp_action_nw_tos(nw_tos=DSCP_BEST_EFF), of.ofp_action_output(port=1)])

        elif self.connection.dpid == 2:
            # Forward everything
            self.send_flow_mod(in_port=1, actions=[of.ofp_action_output(port=2)])
            self.send_flow_mod(in_port=2, actions=[of.ofp_action_output(port=1)])

        log.debug('MoLy setup completed')

def launch ():
    """
    Starts the component
    """
    def start_switch (event):
        log.debug("Controlling %s" % (event.connection,))
        Tutorial(event.connection)
    core.openflow.addListenerByName("ConnectionUp", start_switch)
