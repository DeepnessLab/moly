__author__ = 'orenstal'
import re

try:
    import scapy.all as scapy
except:
    import scapy as scapy

SEND_COMMAND = "send"
QUICK_SEND_COMMAND = "qsend"
QUICK_SEND_FAT_TREE_COMMAND = "qsendf"
REG_PACKET_SEND_COMMAND = "rsend"
EXIT_COMMAND = "exit"
MINSIZE = 0

def simple_tcp_packet(pktlen=100,
                      dl_dst='00:00:00:00:00:07',
                      dl_src='00:00:00:00:00:06',
                      dl_vlan_enable=False,
                      dl_vlan=0,
                      dl_vlan_pcp=60000,
                      dl_vlan_cfi=0,
                      ip_src='10.0.0.10',
                      ip_dst='10.0.0.11',
                      ip_tos=0,
                      tcp_sport=1234,
                      tcp_dport=80,
                      ip_ihl=None,
                      ip_options=False
                      ):
    """
    Return a simple dataplane TCP packet
    Supports a few parameters:
    @param len Length of packet in bytes w/o CRC
    @param dl_dst Destinatino MAC
    @param dl_src Source MAC
    @param dl_vlan_enable True if the packet is with vlan, False otherwise
    @param dl_vlan VLAN ID
    @param dl_vlan_pcp VLAN priority
    @param ip_src IP source
    @param ip_dst IP destination
    @param ip_tos IP ToS
    @param tcp_dport TCP destination port
    @param ip_sport TCP source port
    Generates a simple TCP request.  Users
    shouldn't assume anything about this packet other than that
    it is a valid ethernet/IP/TCP frame.
    """

    if MINSIZE > pktlen:
        pktlen = MINSIZE

    if (dl_vlan_enable):
        pkt = scapy.Ether(dst=dl_dst, src=dl_src)/ \
              scapy.Dot1Q(prio=dl_vlan_pcp, id=dl_vlan_cfi, vlan=dl_vlan)/ \
              scapy.IP(src=ip_src, dst=ip_dst, tos=ip_tos, ihl=ip_ihl)/ \
              scapy.TCP(sport=tcp_sport, dport=tcp_dport)

    else:
        if not ip_options:
            pkt = scapy.Ether(dst=dl_dst, src=dl_src)/ \
                  scapy.IP(src=ip_src, dst=ip_dst, tos=ip_tos, ihl=ip_ihl)/ \
                  scapy.TCP(sport=tcp_sport, dport=tcp_dport)
        else:
            pkt = scapy.Ether(dst=dl_dst, src=dl_src)/ \
                  scapy.IP(src=ip_src, dst=ip_dst, tos=ip_tos, ihl=ip_ihl, options=ip_options)/ \
                  scapy.TCP(sport=tcp_sport, dport=tcp_dport)

    pkt = pkt/("D" * (pktlen - len(pkt)))

    return pkt



def create_eth_packet_with_vlan(pktlen=60, dl_src=None, dl_dst=None, dl_vlan=0, dl_vlan_pcp=2, dl_vlan_cfi=0):

    print "fields are: dst: %s, src: %s, prio: %d, id: %d, vlan: %d" %(dl_dst, dl_src, dl_vlan_pcp, dl_vlan_cfi, dl_vlan)

    if dl_dst is None or dl_src is None:
        return None

    if MINSIZE > pktlen:
        pktlen = MINSIZE

    pkt = scapy.Ether(type=0x800, dst='ff:ff:ff:ff:ff:ff', src='00:00:00:00:00:01')/scapy.Dot1Q(vlan=int(dl_vlan))/scapy.Dot1Q(vlan=int(dl_vlan))/"TEST!!!!!!!!"

    # create the packet data (junk data)
    pkt = pkt/("v" * (pktlen - len(pkt)))

    print "send the following vlan packet: %s" %(str(scapy.Ether(str(pkt)).show()))
    print pkt.payload
    return pkt


def extractMatchFields(fields):
    matchFields = []
    for match in re.findall(r"([\s]*([^=\s]+)[\s]*=[\s]*([^\s,]+)),?[\s]*", fields):
        matchFields.append((match[1], match[2]))

    return matchFields


menuMsg =  "\nPlease enter one of the following options:\n"
menuMsg += "send {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=} - default dl_dst is 00:00:00:00:00:07\n"
menuMsg += "qsend <tp_dst> (quick send to h9 in SimpleTree topology)\n"
menuMsg += "qsendf <tp_dst> (quick send to h9 in Fat Tree topology)\n"
menuMsg += "rsend (sends regular tcp packet to '00:00:00:00:00:01', without vlan tag)\n"
menuMsg += "exit\n"
menuMsg += "------------------------------\n"

while True:
    try:
        userInput = raw_input(menuMsg).lower()

        if userInput is None or userInput == "":
            continue

        splitted = userInput.split()

        if splitted[0] == SEND_COMMAND:
            parseMatcher = re.search(r"send[\s]+\{([^\}]*)\}", userInput)
            matchFields = extractMatchFields(parseMatcher.group(1))

            # default values
            dlDst = '00:00:00:00:00:07'
            dlSrc = '00:00:00:00:00:06'
            nwSrc = '10.0.0.10'
            nwDst = '10.0.0.11'
            tpSrc = 1234
            tpDst = 80

            for i in range(len(matchFields)):
                matchName = matchFields[i][0]
                matchValue = matchFields[i][1]

                if matchName == "dl_dst":
                    dlDst = matchValue
                elif matchName == "dl_src":
                    dlSrc = matchValue
                elif matchName == "nw_src":
                    nwSrc = matchValue
                elif matchName == "nw_dst":
                    nwDst = matchValue
                elif matchName == "tp_src":
                    tpSrc = int(matchValue)
                elif matchName == "tp_dst":
                    tpDst = int(matchValue)

            packet = simple_tcp_packet(dl_dst=dlDst, dl_src=dlSrc, ip_src=nwSrc, ip_dst = nwDst, tcp_sport=tpSrc, tcp_dport=tpDst)
            scapy.sendp(packet, count=1)

        elif splitted[0] == QUICK_SEND_COMMAND:
            if len(splitted) == 2:
                try:
                    tcpDport = int(splitted[1])
                except:
                    tcpDport = None

                if tcpDport is not None:
                    packet = simple_tcp_packet(tcp_dport=tcpDport)
                    scapy.sendp(packet, count=1)
                else:
                    print "Invalid parameter"

            else:
                print "Illegal number of arguments."

        elif splitted[0] == QUICK_SEND_FAT_TREE_COMMAND:
            if len(splitted) == 2:
                try:
                    tcpDport = int(splitted[1])
                except:
                    tcpDport = None

                if tcpDport is not None:
                    packet = simple_tcp_packet(tcp_dport=tcpDport, dl_dst='00:00:00:00:00:09')
                    scapy.sendp(packet, count=1)
                else:
                    print "Invalid parameter"

            else:
                print "Illegal number of arguments."

        elif splitted[0] == REG_PACKET_SEND_COMMAND:
            scapy.sendp(scapy.Ether(type=0x800, dst='00:00:00:00:00:01')/scapy.IP()/scapy.TCP()/"TEST!!!!!!!!")

        elif splitted[0] == EXIT_COMMAND:
            break

        else:
            print "Illegal command"
    except:
        print "Failed to send packet"