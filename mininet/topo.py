from mininet.topo import Topo

class DpiTest(Topo):
    def __init__( self ):
        Topo.__init__(self)

        # Add hosts and switches
        h1 = self.addHost('h1')
        sw1 = self.addSwitch('s1')
        dpi = self.addHost('dpi')
        sw2 = self.addSwitch('s2')
        mbox1 = self.addHost('mbox1')
        h2 = self.addHost('h2')

        # Add links
        self.addLink(h1, sw1)
        self.addLink(sw1, dpi)
        self.addLink(sw1, mbox1)
        self.addLink(sw1, sw2)
        self.addLink(sw2, h2)


class DpiTwoMBs(Topo):
    def __init__( self ):
        Topo.__init__(self)

        # Add hosts and switches
        h1 = self.addHost('h1')
        sw1 = self.addSwitch('s1')
        dpi = self.addHost('dpi')
        sw2 = self.addSwitch('s2')
        mbox1 = self.addHost('mbox1')
        mbox2 = self.addHost('mbox2')
        h2 = self.addHost('h2')

        # Add links
        self.addLink(h1, sw1)
        self.addLink(sw1, dpi)
        self.addLink(sw1, mbox1)
        self.addLink(sw1, sw2)
        self.addLink(sw1, mbox2)
        self.addLink(sw2, h2)


topos = { 'DpiTest': ( lambda: DpiTest() ),
          'DpiTwoMBs': (lambda: DpiTwoMBs() ) }
