""" Final Project - Topologies File
This file was created as part of the course Workshop in Communication Networks
in the Hebrew University of Jerusalem.
"""

from mininet.topo import Topo

class Edge:
    """Represents an edge between two entities"""
    
    def __init__(self, left, right):
        self.left = left
        self.right = right

class FatTreeTopology(Topo):
    """ This topology class emulates a fat-tree topology."""

    def __init__(self, core=4, spine=6, hostPerLeaf=2):
        Topo.__init__(self)

        leaf = 2 * spine
        spineToLeaf = 4

        idx = 1

        # Create Switches
        cores = []
        spines = []
        leaves = []

        for i in range(core):
            cores.append(self.addSwitch('s' + str(idx)))
            idx = idx + 1

        for i in range(spine):
            spines.append(self.addSwitch('s' + str(idx)))
            idx = idx + 1

        for i in range(leaf):
            leaves.append(self.addSwitch('s' + str(idx)))
            idx = idx + 1

        # Create hosts
        hosts = []
        for i in range(hostPerLeaf * leaf):
            hosts.append(self.addHost('h' + str(i + 1)))
        pass


        edges = []
        # Core to spine edges
        for c in cores:
            for s in spines:
                self.addLink(c, s)

        # Spine to leaf edges
        leafPerSpine = leaf / spine
        dir = 0 # 0 - forward, 1 - backward
        for i in range(spine):
            for j in range(spineToLeaf):
                if spineToLeaf == leafPerSpine:
                    self.addLink(spines[i], leaves[i * spineToLeaf + j])
                elif spineToLeaf > leafPerSpine:
                    self.addLink(spines[i], leaves[(i - (i % leafPerSpine)) * leafPerSpine + j])
            dir = 1 - dir

        # Leaf to host edges
        for i in range(leaf):
            for j in range(hostPerLeaf):
                self.addLink(leaves[i], hosts[i * hostPerLeaf + j])

topos = { 'FatTree': FatTreeTopology }
