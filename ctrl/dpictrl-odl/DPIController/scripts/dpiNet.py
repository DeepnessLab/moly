from subprocess import call
from mininet.topo import Topo
from mininet.topolib import TreeTopo
from mininet.cli import CLI
from mininet.log import lg
from mininet.node import Node
from mininet.node import RemoteController
from mininet.node import OVSSwitch
from mininet.net import Mininet
from mininet.util import customConstructor, splitArgs
import argparse
import mininet.term
from mininet.term import makeTerm
import time
from fatTree import FatTreeTopology
from mobility import *

def generateTopology(args):
    args = args.split(',')
    return FatTreeTopology(*map(int,args))    
    

def connectSDN2DPI(net,controllerHost):
	c0 = net.get('c0')
	net.addLink(c0,controllerHost)
	controllerHost.cmd('ifconfig h1-%s 10.100'%(controllerHost))
	c0.cmd('ifconfig c0-eth0 10.101')
	controllerHost.cmd('route add -host 10.0.0.101 dev h1-eth1')
	print 'link added between sdn-controller and dpiControllerHost'

def startMininet(topo):    
	switch = MobilitySwitch
	controller = RemoteController
	net = Mininet(topo=topo,switch=switch,controller = controller)
	net.start()
	return net

def parseArgs():
	parser = argparse.ArgumentParser()
	parser.add_argument("--dpiControllerHost",help="host of the dpi controller within the mininet (default h1)",default='h1')
	parser.add_argument("--topo",help="fatTree topology args",default='2,2,2')
	parser.add_argument("--middlebox","--mb",nargs='+',help="which mininet hosts should contain middlebox (host[=MatchRulesFile])")
	parser.add_argument("--instance",nargs='+',help="which mininet hosts should contain middlebox (host[=MatchRules])")
	parser.add_argument("--xterms",help="open xterms for middleboxes and instances",action='store_true')
	args = parser.parse_args()	
	return args


def launchDPINetwork(args,net):
    print 'starting controller..'
      
    net.get(args.dpiControllerHost).cmd("java -jar bin/controller.jar &")
    time.sleep(10)
    if(args.middlebox):
        print 'starting middleboxes:'
        for mb in args.middlebox:         
            mb = mb.split('=')
            print 'starting '+mb[0]
            cmd = 'java -jar bin/IDSWrapper.jar -id %s '%(mb[0])
            if len(mb) > 1:
                cmd += '--rules '+mb[1]
	    else:
		cmd = 'xterm -T %s -e %s --interactive'%(mb[0],cmd)
		mininet.term.tunnelX11(net.get(mb[0]))
            net.get(mb[0]).cmd(cmd + ' &')
            if args.xterms:
		host = net.get(mb[0])
		print 'open xterm'
		mininet.term.tunnelX11(host)
		host.cmd('xterm -T %s -e ngrep -t -d %s-eth0 -v "arp and lldp" &'%(mb[0],mb[0]))
            time.sleep(5)

    
    if(args.instance):
        print 'starting instances:'
        for instance in args.instance:
            instance = instance.split('=')
            print 'starting '+instance[0]
            net.get(instance[0]).cmd('java -jar bin/serviceWrapper.jar -id %s &'%(instance[0]))
            if args.xterms:
		print 'open xterm'
		host = net.get(instance[0])
		mininet.term.tunnelX11(host)
		host.cmd("xterm -T %s -e ngrep -t -d %s-eth0 -v 'arp and lldp' &"%(instance[0],instance[0]))
            time.sleep(5)
            		
if __name__ == '__main__':	
    args = parseArgs()
    print 'generating topology..'
    topo = generateTopology(args.topo)
    print 'starting mininet..'
    net = startMininet(topo)
    time.sleep(10)
    print 'Pinging for network connectivity..'
    net.pingAll()
    controllerHost = net.get(args.dpiControllerHost)
    print 'connecting controller host to sdn controller'
    connectSDN2DPI(net,controllerHost)
    time.sleep(2)
    print "starting DPI network"
    launchDPINetwork(args,net)
    cli = CLI( net )
    net.stop()
    print 'Cleaning..'
    call(['sudo','mn','-c'])

