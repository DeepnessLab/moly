"""
Tal Orenstein
301293767
orenstal
--------------------------
A set of utils for the OpenFlow exercise.
This file was created as part of the course Workshop in Communication Networks
in the Hebrew University of Jerusalem.
"""

import threading

class UnionFind:
    """ This class implements the operations on the disjoint-set data structure.
    The operations are performed on any Python object, by adding two additional
    fields on each set element object given. 
    Based on code from here: http://code.activestate.com/recipes/577225-union-find/
    """
    
    class RootElm:
        def __init__(self):
            self.uf_rank = 0
            self.uf_parent = self

    @staticmethod
    def make_set(x):
        """ Creates a singleton set out of some given object.
        """
        x.uf_parent = UnionFind.RootElm()

    @staticmethod
    def union(x, y):
        """ Unions two set elements (objects) to the same set.
        If elements are in the same set, does nothing.
        """
        xRoot = UnionFind.find(x)
        yRoot = UnionFind.find(y)
        if xRoot.uf_rank > yRoot.uf_rank:
            yRoot.uf_parent = xRoot
        elif xRoot.uf_rank < yRoot.uf_rank:
            xRoot.uf_parent = yRoot
        elif xRoot != yRoot:
            yRoot.uf_parent = xRoot
            xRoot.uf_rank = xRoot.uf_rank + 1

    @staticmethod
    def find(x):
        """ Finds the set root element of a given element (object).
        Also compresses the path from the element to its root.
        """
        if x.uf_parent == x:
            return x
        else:
            x.uf_parent = UnionFind.find(x.uf_parent)
            return x.uf_parent

class SingletonType(type):
    """ A singleton metaclass """
    def __init__(cls, name, bases, dictionary):
        super(SingletonType, cls).__init__(name, bases, dictionary)
        cls._instance = None
        cls._rlock = threading.RLock()

    def __call__(cls, *args, **kws):
        if cls._instance is not None:
            return cls._instance
        with cls._rlock:
            if cls._instance is None:
                cls._instance = super(SingletonType, cls).__call__(*args, **kws)
        return cls._instance

## Example: How to create a singleton class:
# class MyOwnSingletonClass:
#     __metaclass__ = utils.SingletonType
# #... rest of class ...

class Graph:
    def __init__(self):
        self.nodes = {}
        self.edges = {}
    
    def add_node(self, label, data):
        if label not in self.nodes:
            self.nodes[label] = data
    
    def add_edge(self, a, b, data):
        if (a, b) not in self.edges and (b, a) not in self.edges:
            self.edges[(a, b)] = data
    
    def delete_edge(self, a, b):
        if (a, b) in self.edges:
            self.edges.pop((a, b), None)
            return True
        if (b, a) in self.edges:
            self.edges.pop((b, a), None)
            return True

        return False

    def deleteEdge(self, edge):
        self.edges.pop(edge, None)
        
    def get_edge(self, a, b):
        if (a, b) in self.edges:
            return self.edges[(a, b)]
        elif (b, a) in self.edges:
            return self.edges[(b, a)]
        else:
            return None

    def getEdge(self, edge):
        if edge in self.edges:
            return self.edges[edge]

        return None

    
    def remove_node(self, label):
        self.nodes.pop(label, None)

    def removeAllEdgesOfNode(self, nodeLabel):
        for n in self.nodes:
            if self.get_edge(n, nodeLabel) <> None:
                self.delete_edge(n, nodeLabel)

    def getNode(self, nodeLabel):
        if nodeLabel in self.nodes:
            return self.nodes[nodeLabel]

        return None

    def getEdges(self):
        return self.edges

    def getNodes(self):
        return self.nodes

    def setEdges(self, edges):
        self.edges = edges

    def setNodes(self, nodes):
        self.nodes = nodes



        
class Timer:
    def __init__(self, interval, function, args = [], recurring = False):
        self.interval = interval
        self.func = function
        self.args = args
        self.recurring = recurring
        self.active = False
        # starting automatically
        self.start()
    
    def __do_func__(self):
        self.func(*self.args)
        if (self.recurring and self.active):
            self.start()
    
    def start(self):
        self.active = True
        self.timer = threading.Timer(self.interval, self.__do_func__)
        self.timer.start()
    
    def stop(self):
        if (self.timer is not None):
            self.timer.cancel()
        self.active = False
    