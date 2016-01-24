__author__ = 'orenstal'

from utils import *
import urllib2
import json
import socket
import sys
import re
import logging


def printLogMsg(level, functionName, msg):
    msgToLog = '[' + str(functionName) + '] ' + str(msg)

    if level == logging.CRITICAL:
        logging.critical(msgToLog)
    elif level == logging.ERROR:
        logging.error(msgToLog)
    elif level == logging.WARNING:
        logging.warning(msgToLog)
    elif level == logging.INFO:
        logging.info(msgToLog)
    elif level == logging.DEBUG:
        logging.debug(msgToLog)


class tsaBE(object):

    # commands between the tsa BE and the DPI controller
    UNREGISTER_MIDDLEBOX_DPI_CONT_COMMAND = "unregistermiddleboxcommand"
    REPLACE_DPI_INSTANCE_DPI_CONT_COMMAND = "replacedpiinstancecommand"
    ADD_POLICY_CHAIN_DPI_CONT_COMMAND = "addpolicychaincommand"
    REMOVE_POLICY_CHAIN_DPI_CONT_COMMAND = "removepolicychaincommand"
    PRINT_DPI_CONTROLLER_STATUS_DPI_CONT_COMMAND = "printdpicontrollerstatuscommand"
    PRINT_DPI_CONTROLLER_FULL_STATUS_DPI_CONT_COMMAND = "printdpicontrollerfullstatuscommand"

    # commands between the tsa FE and the tsa BE
    ADD_POLICY_CHAIN_CLI = "addpolicychain"
    REMOVE_POLICY_CHAIN_CLI = "removepolicychain"
    PRINT_DPI_CONTROLLER_STATUS_CLI = "printdpicontroller"
    PRINT_DPI_CONTROLLER_FULL_STATUS_CLI = "printdpicontrollerfull"
    EXIT_CLI = "exit"

    # parameters for json communication
    COMMAND_STRING = "command"
    ARGUMENTS_COMMAND = "arguments"
    DATA_STRING = "data"
    RETURN_VALUE_STRING = "return value"
    SUCCESS_STRING = "success"
    FAILED_STRING = "failed"
    POLICY_CHAIN_STRING = "policy chain"
    POLICY_CHAIN_ID_STRING = "pcid"

    # parameters for rules installation
    FLOW_DEFAULT_PRIORITY = 60000
    REST_PRIORITY = "priority"

    # parameters for the tsaBE server
    LISTENING_IP = '10.0.0.101'
    LISTENING_PORT = 9093

    # parameters to communicate with the SDN controller
    SDN_CONTROLLER_IP = "127.0.0.1"
    SDN_CONTROLLER_PORT = '8080'
    SDN_CONTROLLER_URI = "http://" + SDN_CONTROLLER_IP + ":" + SDN_CONTROLLER_PORT


    def __init__ (self, dpiControllerIp, dpiControllerPort):
        self.dpiControllerIp = dpiControllerIp
        self.dpiControllerPort = dpiControllerPort


        # The network's switches graph.
        # Each node is represented by a key-value mapping, such that the key is the switch dpid and the value is a map
        # with 2 keys: {'neighbors': [], 'isVisited': False}
        #   - 'neighbors': a list of all the neighbor's dpid and the port to reach them (for example, if we s1 is connected
        #                  to s1 via port 3 [i.e., (s1,3) -> s2], thus the neighbors list of s1 will contain this data:
        #                  [3, s2 dpid]
        #   - 'isVisited': a boolean flag for finding a path in the graph.
        #
        # Each edge is represented by a key-value mapping. the key is a tuple of size 2 with the two nodes, and the
        # value is always empty.
        # Note that the edges are bidirectional
        self.graph = Graph()


        # A mapping between the policyChain key to a list of data about this policy chain.
        # A policyChain key is composed of the sender host, the middleboxes policyChain and the match fields. For example,
        # if the user inserts this line: addPolicyChain h1,m8,m3 {tp_src=3334}, the policyChainKey will be:
        # u'h1$m8,m3$tp_dst:3334'
        #
        # The list of data is composed of a list of:
        #   - the policychain without the sender host but with and the dpi instance (i.e. for the example above, this
        #     cell has this value: [u'm6', u'm8', u'm3'], while m6 is the chosen dpi instance).
        #   - the vlan id
        #   - the full flow of switches for this policychain. An example to full flow for the example above can be:
        #     [u'$h1', u'000000000000000b', u'0000000000000006', u'000000000000000d', u'$m6', u'000000000000000d',
        #      u'0000000000000006', u'000000000000000e', u'$m8', u'000000000000000e', u'0000000000000006',
        #      u'000000000000000c']]
        #     Note that the full flow contains the host/middlebox name when we reach it (thus the first cell is always
        #     the sender host).
        #
        # Complete example for the command above may be:
        # {u'h1$m8,m3$tp_dst:3334': [[u'm6', u'm8', u'm3'], 2,
        # [u'$h1', u'000000000000000b', u'0000000000000006', u'000000000000000d', u'$m6', u'000000000000000d',
        # u'0000000000000006', u'000000000000000e', u'$m8', u'000000000000000e', u'0000000000000006', u'000000000000000c']]
        self.policyChainKeyToData = {}


        # Each policychain has a pcid (policy chain id). the pcid is sent and stored in the DPI controller, for common
        # language. By that, the tsa hide his policyChainKey (that may change in case of unregister middlebox) from the
        # DPI controller
        self.nextPcid = 0
        self.pcidToPolicyChainKey = {}


        # middlebox name -> [switch dpid (which is connected to), switch port]
        self.middleboxToSwitchMapping = {}

        # host name -> [switch dpid (which is connected to), switch port]
        self.hostsThatSendPackets = {}

        # Gets true when self.middleboxToSwitchMapping and self.hostsThatSendPackets are not empty
        self.isReady = False

        self.nextVlanId = 1

        self.fillMBToSwitchMapping()
        self.buildGraph()
        self.printGraph()

        self.runServer()



    def fillMBToSwitchMapping(self):
        """
        This method read the configuration file named 'tsaConfigFile.txt' which is located on the same directory as this
        file, and fill the mapping between the middleboxes to the switches they are connected to.
        The assumed format of the configuration file is:
            - Each middlebox in the network should has a different line in the following format:
              <host_id> <switch_name_that_the_middlebox_is_connected_to> <host_mac_address>
              For example: for host h2, the line should look like:
              2 s11 00:00:00:00:00:02
            - An empty line
            - Each sender host in the network should has a different line in the following format:
              <host_id> <switch_name_that_the_host_is_connected_to> <host_mac_address>
              For example: for host h1, the line should look like:
              1 s11 00:00:00:00:00:01
            - In addition, you are allowed to add comments. A comment should be in new line and start with '#' sign.
        """

        # instead of requesting the data for each switch separately from the SDN controller, it asks the data for all
        # the switches
        switchesResponse = urllib2.urlopen(tsaBE.SDN_CONTROLLER_URI + '/v1.0/topology/switches')
        switchesData = json.load(switchesResponse)


        with open("tsaConfigFile.txt") as f:
            isSendPackets = False

            for line in f:
                if line is None or len(line) <= 2:
                    isSendPackets = True
                    continue

                if line[0] == "#":
                    continue

                splitted = line.split()
                mbName = 'm'+splitted[0]
                switchName = splitted[1]
                mbMac = splitted[2]

                result = self.fillMBToSwitchForSpecificMB(switchesData, mbName, mbMac, switchName)

                if result is not None:
                    if isSendPackets:
                        hostName = 'h' + result[0][1:]
                        self.hostsThatSendPackets[hostName] = [result[1], result[2]]
                    else:
                        self.addMBToSwitchEntry(result[0], result[1], result[2])

        printLogMsg(logging.DEBUG, "fillMBToSwitchMapping", "The mapping between middleboxes to their switches is: %s" % str(self.middleboxToSwitchMapping))
        printLogMsg(logging.DEBUG, "fillMBToSwitchMapping", "Sender hosts are: %s" % str(self.hostsThatSendPackets))

        if self.hostsThatSendPackets == {} or self.middleboxToSwitchMapping == {}:
            printLogMsg(logging.CRITICAL, "fillMBToSwitchMapping", "The network is still not stable. Please run pingall")
        else:
            self.isReady = True



    def fillMBToSwitchForSpecificMB(self, switchesData, mbName, mbMac, switchName):
        printLogMsg(logging.DEBUG, "fillMBToSwitchForSpecificMB", "The received arguments are: mbName: %s, mbMac: %s, switchName: %s" %(str(mbName), str(mbMac), str(switchName)))

        # iterate over the switches's data and search for match
        for d in range(len(switchesData)):
            switchDpid = switchesData[d]['dpid']
            portName = switchesData[d]['ports'][0]['name']

            switchNameMatcher = re.search(r'(.*)-', portName)

            if switchNameMatcher and switchName == switchNameMatcher.group(1):
                # ask for all the installed flows of this switch
                url = tsaBE.SDN_CONTROLLER_URI + '/stats/flow/' + str(int(switchDpid, 16))
                switchRulesResponse = urllib2.urlopen(url)
                rulesData = json.load(switchRulesResponse)

                rules = rulesData[str(int(switchDpid, 16))]

                # search the leading port to the received middlebox
                for ruleIndex in range(len(rules)):
                    if str(rules[ruleIndex]['match']['dl_dst']) == str(mbMac):
                        switchPortToMB = rules[ruleIndex]['actions'][0][7:]

                        return [mbName, switchDpid, switchPortToMB]

        return None



    def addMBToSwitchEntry(self, middleboxName, switchDpid, switchPort):
        self.middleboxToSwitchMapping[middleboxName] = [switchDpid, switchPort]



    def buildGraph(self):
        newGraph = Graph()

        # get a list of all the switches
        response = urllib2.urlopen(tsaBE.SDN_CONTROLLER_URI + '/v1.0/topology/switches')
        data = json.load(response)

        # add the nodes to the graph (the switch's dpid)
        for d in range(len(data)):
            newGraph.add_node(data[d]['dpid'], {'neighbors': [], 'isVisited': False})

        # get all the links in the graph
        response = urllib2.urlopen(tsaBE.SDN_CONTROLLER_URI + '/v1.0/topology/links')
        data = json.load(response)

        # add the edges to the graph (the links between the switches)
        for d in range(len(data)):
            src = data[d]['src']
            dst = data[d]['dst']

            newGraph.add_edge(src['dpid'], dst['dpid'], {})

            # update source switch neighbors (if necessary)
            srcNeighbors = (newGraph.getNode(src['dpid']))['neighbors']
            if not self.isNeighborExist(srcNeighbors, src['port_no'], dst['dpid']):
                srcNeighbors.append([src['port_no'], dst['dpid']])

            # update the destination switch neighbors (if necessary)
            dstNeighbors = (newGraph.getNode(dst['dpid']))['neighbors']
            if not self.isNeighborExist(dstNeighbors, dst['port_no'], src['dpid']):
                dstNeighbors.append([dst['port_no'], src['dpid']])

        self.removeOneDirectionLinks(newGraph)

        self.graph = newGraph



    def isNeighborExist(self, neighbors, viaPort, toDestDpid):
        for i in range(len(neighbors)):
            if neighbors[i][1] == toDestDpid:
                if viaPort != None:
                    if neighbors[i][0] == viaPort:
                        return True
                else:
                    return True

        return False



    def removeOneDirectionLinks(self, graph):
        """
        There is a bug in rest_topology. sometimes it returns not existing links (for Fat Tree topology).
        I have noticed that the not existing links are shown only in one direction (for example, the rest_topologu returns
        the s1 is a neighbor of s2, but s2 is not neighbor of s1).
        Therefor, this function goal is to remove one direction links from the received graph.
        """

        for nodeDpid, nodeData in graph.getNodes().items():
            neighborsCopy = list(nodeData['neighbors'])

            for i in range(len(neighborsCopy)):
                neighborDpid = neighborsCopy[i][1]

                if graph.getNode(neighborDpid) is None or not self.isNeighborExist(graph.getNode(neighborDpid)['neighbors'], None, nodeDpid):
                    # remove neighbor from the neighbors list
                    graph.getNode(nodeDpid)['neighbors'].remove(neighborsCopy[i])

                    # delete the edge between them
                    graph.delete_edge(nodeDpid, neighborDpid)



    def printGraph(self):
        printLogMsg(logging.INFO, "printGraph", "----------------------------------")
        printLogMsg(logging.INFO, "printGraph", "nodes:")

        nodes = self.graph.getNodes()
        for node in nodes:
            printLogMsg(logging.INFO, "printGraph", "%s -> %s" %(str(node), str(nodes[str(node)])))

        printLogMsg(logging.INFO, "printGraph", "----------------------------------")
        printLogMsg(logging.INFO, "printGraph", "edges:")
        printLogMsg(logging.INFO, "printGraph", str(self.graph.getEdges()))
        printLogMsg(logging.INFO, "printGraph", "----------------------------------")



    def runServer(self):
        serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        serversocket.bind((tsaBE.LISTENING_IP, tsaBE.LISTENING_PORT))
        serversocket.listen(5)

        while True:
            connection, address = serversocket.accept()
            buff = connection.recv(8096) #8k
            if len(buff) > 0:
                printLogMsg(logging.DEBUG, "runServer", "received line: %s" %str(buff))

                if self.isReady == False:
                    self.fillMBToSwitchMapping()

                    # check if now the TSA is ready
                    if self.isReady == False:
                        response = self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Please run pingall')
                        connection.send(json.dumps(response) + '\n')
                        continue

                if buff is None or buff == "":
                    continue

                request = json.loads(buff.lower())

                command = request[tsaBE.COMMAND_STRING]
                requestArgs = request[tsaBE.ARGUMENTS_COMMAND]

                if command == tsaBE.ADD_POLICY_CHAIN_CLI:
                    printLogMsg(logging.INFO, "runServer", "processing 'add policychain' command")
                    response = self.handleAddPolicyCommand(requestArgs)

                elif command == tsaBE.REMOVE_POLICY_CHAIN_CLI:
                    printLogMsg(logging.INFO, "runServer", "processing 'remove policychain' command")
                    response = self.handleRemovePolicychain(requestArgs)

                elif command == tsaBE.UNREGISTER_MIDDLEBOX_DPI_CONT_COMMAND:
                    printLogMsg(logging.INFO, "runServer", "processing 'unregister middlebox' command")
                    response = self.handleUnregisterMiddleboxCommand(requestArgs)

                elif command == tsaBE.REPLACE_DPI_INSTANCE_DPI_CONT_COMMAND:
                    printLogMsg(logging.INFO, "runServer", "processing 'replace dpi instance' command")
                    response = self.handleReplacingDPIInstanceCommand(requestArgs)

                elif command == tsaBE.PRINT_DPI_CONTROLLER_STATUS_CLI:
                    printLogMsg(logging.INFO, "runServer", "processing 'print dpi controller' command")
                    request = self.toJSONRequestFormat(tsaBE.PRINT_DPI_CONTROLLER_STATUS_DPI_CONT_COMMAND, {})
                    self.sendToDPIController(request)
                    response = self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, '')

                elif command == tsaBE.PRINT_DPI_CONTROLLER_FULL_STATUS_CLI:
                    printLogMsg(logging.INFO, "runServer", "processing 'print full dpi controller' command")
                    request = self.toJSONRequestFormat(tsaBE.PRINT_DPI_CONTROLLER_FULL_STATUS_DPI_CONT_COMMAND, {})
                    self.sendToDPIController(request)
                    response = self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, '')

                elif command == tsaBE.EXIT_CLI:
                    response = self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, '')
                    connection.send(json.dumps(response) + '\n')
                    break
                else:
                    response = self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Illegal command')
                    printLogMsg(logging.WARNING, "runServer", "Illegal command")

                printLogMsg(logging.INFO, "runServer", "Send back this msg: %s" %str(response))
                connection.send(json.dumps(response) + '\n')

        serversocket.close()



    def handleAddPolicyCommand(self, requestArgs):
        try:
            # parse the request in order to extract the policy chain and the match fields without spaces
            parseMatcher = re.search(r"(([\w]+[\s]*,[\s]*)*[\w]+)[\s]+\{([^\}]*)\}", requestArgs)
            [hostSender, preparedPolicyChain] = self.preparePolicyChainWithHostToSend(parseMatcher.group(1))
            matchFields = self.extractMatchFields(parseMatcher.group(3))

            pcid = self.nextPcid

            arguments = {}
            arguments[tsaBE.POLICY_CHAIN_STRING] = preparedPolicyChain
            arguments[tsaBE.POLICY_CHAIN_ID_STRING] = str(pcid)

            request = self.toJSONRequestFormat(tsaBE.ADD_POLICY_CHAIN_DPI_CONT_COMMAND, arguments)

            result = self.sendToDPIController(request)

            if result[tsaBE.RETURN_VALUE_STRING].lower() == tsaBE.SUCCESS_STRING:
                # the new policychain contains also the assigned dpi instance
                newPolicyChain = result[tsaBE.DATA_STRING]
                newPolicyChain = newPolicyChain.split(',')

                vlanId = self.getVlanId()
                policyChainKey = self.generatePolicyChainKey(hostSender, preparedPolicyChain, matchFields)
                self.installPolicyChainRules(policyChainKey, newPolicyChain, vlanId, matchFields, hostSender)

                self.pcidToPolicyChainKey[pcid] = self.generatePolicyChainKey(hostSender, preparedPolicyChain, matchFields)
                self.nextPcid += 1

                printLogMsg(logging.DEBUG, "handleAddPolicyCommand", "policyChainKeyToData: %s" %str(self.policyChainKeyToData))
                printLogMsg(logging.DEBUG, "handleAddPolicyCommand", "pcidToPolicyChainKey: %s" %str(self.pcidToPolicyChainKey))

                return self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, 'Policy chain: %s was added successfully. vlanId is: %s' %(parseMatcher.group(1), str(vlanId)))
            else:
                return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to add policy chain: %s.' %(parseMatcher.group(1)))
        except:
            return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to add policy chain.')



    def handleRemovePolicychain(self, requestArgs):
        try:
            # parse the request in order to extract the policy chain and the match fields without spaces
            parseMatcher = re.search(r"(([\w]+[\s]*,[\s]*)*[\w]+)[\s]+\{([^\}]*)\}", requestArgs)
            [hostSender, preparedPolicyChain] = self.preparePolicyChainWithHostToSend(parseMatcher.group(1))

            matchFields = self.extractMatchFields(parseMatcher.group(3))
            policyChainKey = self.generatePolicyChainKey(hostSender, preparedPolicyChain, matchFields)

            return self.removePolicyChain(policyChainKey, True)
        except:
            return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to remove policy chain.')



    def handleUnregisterMiddleboxCommand(self, requestArgs):
        return self.unregisterMiddlebox(requestArgs)



    def handleReplacingDPIInstanceCommand(self, requestArgs):
        splitted = requestArgs.split()

        if len(splitted) == 2:
            pcid = int(splitted[1])
            if pcid in self.pcidToPolicyChainKey:
                printLogMsg(logging.DEBUG, "handleReplacingDPIInstanceCommand", "pcid  is %d, and its policychain is: %s" %(pcid, str(self.pcidToPolicyChainKey[pcid])))
                return self.replaceDPIInstance(splitted[0], self.pcidToPolicyChainKey[pcid])



    def getVlanId(self):
        vlanId = self.nextVlanId
        self.nextVlanId += 1

        return vlanId



    def findPathBetweenNodes(self, srcDpid, dstDpid):
        printLogMsg(logging.DEBUG, "findPathBetweenNodes", "find path between %s to %s" %(str(srcDpid), str(dstDpid)))
        self.turnOffVisitedBit()
        return self.getPath(srcDpid, dstDpid)

    def turnOffVisitedBit(self):
        for node in self.graph.getNodes():
            self.graph.getNode(node)['isVisited'] = False


    def getPath(self, srcDpid, dstDpid):
        # If we already have visited this node, returns None
        if self.graph.getNode(srcDpid)['isVisited'] == True:
            return None

        # set 'isVisited' bit to be true
        self.graph.getNode(srcDpid)['isVisited'] = True

        # If we found the target, return target in a list
        if str(srcDpid) == str(dstDpid):
            return [srcDpid]

        # recursively iterate over children
        srcNeighbors = self.graph.getNode(srcDpid)['neighbors']

        for neighbor in srcNeighbors:
            neighborDpid = neighbor[1]

            tail = self.getPath(neighborDpid, dstDpid)

            if tail is not None:
                return [srcDpid] + tail

        return None



    def installPolicyChainRules(self, policyChainKey, policyChain, vlanId, matchFields, senderHost):
        """
        Note: policyChain is a list of middleboxes names including the dpi instance but without the sender host
        """

        printLogMsg(logging.DEBUG, "installPolicyChainRules", "Received arguments are: policyChainKey: %s, policyChain; %s, vlanId: %s, matchFields: %s, senderHost: %s" %(str(policyChainKey), str(policyChain), str(vlanId), str(matchFields), senderHost))

        self.policyChainKeyToData[policyChainKey] = [policyChain, vlanId, ["$"+senderHost]]

        # install rules from sender packets host (for example, h8) to dpi instance
        self.installRulesToDpiInstance(policyChainKey, policyChain[0], vlanId, senderHost, matchFields)

        for i in range(len(policyChain) -1):
            printLogMsg(logging.DEBUG, "installPolicyChainRules", "\ni: %d, policyChain[i]: %s" %(i, str(policyChain[i])))

            if i == len(policyChain)-2:
                self.installPolicyChainBetweenTwoMiddleboxes(policyChainKey, policyChain[i], policyChain[i+1], vlanId, True)
            else:
                self.installPolicyChainBetweenTwoMiddleboxes(policyChainKey, policyChain[i], policyChain[i+1], vlanId, False)



    def installPolicyChainBetweenTwoMiddleboxes(self, policyChainKey, srcMiddleboxName, dstMiddleboxName, vlanId, shouldInstalVlanPopRule):
        printLogMsg(logging.DEBUG, "installPolicyChainBetweenTwoMiddleboxes", "Received arguments are: policyChainKey: %s, srcMiddleboxName: %s, dstMiddleboxName: %s, vlanId: %s, shouldInstalVlanPopRule: %s" %(str(policyChainKey), str(srcMiddleboxName), str(dstMiddleboxName), str(vlanId), str(shouldInstalVlanPopRule)))
        if srcMiddleboxName not in self.middleboxToSwitchMapping:
            return

        srcSwitchData = self.middleboxToSwitchMapping[srcMiddleboxName]
        switchDpidToSrcMiddlebox = srcSwitchData[0]
        switchPortToSrcMiddlebox = srcSwitchData[1]

        dstSwitchData = self.middleboxToSwitchMapping[dstMiddleboxName]
        switchDpidToDstMiddlebox = dstSwitchData[0]
        switchPortToDstMiddlebox = dstSwitchData[1]

        self.policyChainKeyToData[policyChainKey][2].append("$" + srcMiddleboxName)  # policyChain[i] means the src middlebox

        switchesPath = self.findPathBetweenNodes(switchDpidToSrcMiddlebox, switchDpidToDstMiddlebox)
        printLogMsg(logging.DEBUG, "installPolicyChainBetweenTwoMiddleboxes", "switches path is: %s" %str(switchesPath))

        # the first inPort connects the source middlebox to it's switch
        inPort = switchPortToSrcMiddlebox

        for j in range(len(switchesPath) -1):
            # finding the leading port to the neighbor
            outPort = self.getPortToNeighbor(self.graph.getNode(switchesPath[j])['neighbors'], switchesPath[j+1])
            printLogMsg(logging.DEBUG, "installPolicyChainBetweenTwoMiddleboxes", "The leading port of %s to his neighbor %s, is: %s" %(str(switchesPath[j]), str(switchesPath[j+1]), str(outPort)))

            self.installVlanRule(policyChainKey, switchesPath[j], vlanId, outPort, inPort)

            # keep the incoming port for the next switch
            inPort = self.getPortToNeighbor(self.graph.getNode(switchesPath[j+1])['neighbors'], switchesPath[j])
            printLogMsg(logging.DEBUG, "installPolicyChainBetweenTwoMiddleboxes", "The leading port of %s to his neighbor %s, is: %s" %(str(switchesPath[j+1]), str(switchesPath[j]), str(inPort)))

        if shouldInstalVlanPopRule:
            # install rule for the switch that is connected to the last middlebox (the last switch in the path).
            # this rule should include action to pop the vlan tag
            self.installRuleWithVlanPop(policyChainKey, switchesPath[-1], vlanId, switchPortToDstMiddlebox, inPort)
        else:
            # install the rule for the last switch
            self.installVlanRule(policyChainKey, switchesPath[-1], vlanId, switchPortToDstMiddlebox, inPort)



    def installRulesToDpiInstance(self, policyChainKey, instName, vlanId, senderHost, matchFields):
        printLogMsg(logging.DEBUG, "installRulesToDpiInstance", "Received arguments are: policyChainKey: %s, dpiInstanceName: %s, vlanId: %s, senderHost: %s, matchFields: %s" %(str(policyChainKey), str(instName), str(vlanId), str(senderHost), str(matchFields)))

        switchDpidToPacketsSender = self.hostsThatSendPackets[senderHost][0]
        switchPortToPacketsSender = self.hostsThatSendPackets[senderHost][1]

        dstSwitchData = self.middleboxToSwitchMapping[instName]
        switchDpidToDstMiddlebox = dstSwitchData[0]
        switchPortToDstMiddlebox = dstSwitchData[1]

        switchesPath = self.findPathBetweenNodes(switchDpidToPacketsSender, switchDpidToDstMiddlebox)
        printLogMsg(logging.DEBUG, "installRulesToDpiInstance", "switches path is: %s" %str(switchesPath))

        # the first inPort connects the source middlebox to it's switch
        inPort = switchPortToPacketsSender

        for j in range(len(switchesPath) -1):
            outPort = self.getPortToNeighbor(self.graph.getNode(switchesPath[j])['neighbors'], switchesPath[j+1])
            printLogMsg(logging.DEBUG, "installRulesToDpiInstance", "The leading port of %s to his neighbor %s, is: %s" %(str(switchesPath[j]), str(switchesPath[j+1]), str(outPort)))
            if j == 0:
                self.installRuleWithVlanPush(policyChainKey, switchesPath[j], vlanId, outPort, inPort, matchFields=matchFields)
            else:
                self.installVlanRule(policyChainKey, switchesPath[j], vlanId, outPort, inPort)

            # keep the incoming port for the next switch
            inPort = self.getPortToNeighbor(self.graph.getNode(switchesPath[j+1])['neighbors'], switchesPath[j])
            printLogMsg(logging.DEBUG, "installRulesToDpiInstance", "The leading port of %s to his neighbor %s, is: %s" %(str(switchesPath[j+1]), str(switchesPath[j]), str(inPort)))

        # install the rule for the last switch
        if len(switchesPath) == 1:
            self.installRuleWithVlanPush(policyChainKey, switchesPath[0], vlanId, switchPortToDstMiddlebox, inPort, matchFields=matchFields)
        else:
            self.installVlanRule(policyChainKey, switchesPath[-1], vlanId, switchPortToDstMiddlebox, inPort)



    def getPortToNeighbor(self, neighbors, destDpid):
        for [port, neighborDpid] in neighbors:
            if destDpid == neighborDpid:
                return port

        return None



    def getFullMatchFields(self, matchFields):
        match = {}
        if matchFields is not None:
            for i in range(len(matchFields)):
                match[matchFields[i][0]] = matchFields[i][1]

                if matchFields[i][0] == "tp_dst" or matchFields[i][0] == "tp_src":
                    match["nw_proto"] = "6"
                    match["dl_type"] = "2048"

                elif matchFields[i][0] == "nw_src" or matchFields[i][0] == "nw_dst":
                    match["dl_type"] = "2048"

        printLogMsg(logging.DEBUG, "getFullMatchFields", "The full match is: %s" %str(matchFields))
        return match



    def installVlanRule(self, policyChainKey, switchDpid, vlanId, outPort, inPort=None, priority=FLOW_DEFAULT_PRIORITY):

        if ((switchDpid is not None) and (vlanId is not None) and (outPort is not None)):
            url = tsaBE.SDN_CONTROLLER_URI + '/stats/flowentry/add'

            actions = []
            actions.append({"type":"OUTPUT", "port": str(outPort)})

            match = {}
            match["dl_vlan"] = str(vlanId)

            if inPort is not None:
                match["in_port"] = str(inPort)

            data = {"dpid": str(int(switchDpid, 16)),
                    tsaBE.REST_PRIORITY: str(priority),
                    "match": match,
                    "actions": actions}

            printLogMsg(logging.DEBUG, "installVlanRule", "The url is: %s, and the request content is: %s" %(url, str(data)))

            try:
                req = urllib2.Request(url)
                req.add_header('Content-Type', 'application/json')
                urllib2.urlopen(req, json.dumps(data))
            except:
                printLogMsg(logging.WARNING, "installVlanRule", "[%s] Failed to install a rule: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                                       str(vlanId), str(outPort)))
                return False

            self.policyChainKeyToData[policyChainKey][2].append(switchDpid)
            return True

        printLogMsg(logging.WARNING, "installVlanRule", "[%s] Failed to install a rule: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                               str(vlanId), str(outPort)))
        return None



    def installRuleWithVlanPush(self, policyChainKey, switchDpid, vlanId, outPort, inPort=None, priority=FLOW_DEFAULT_PRIORITY, matchFields=None):

        if ((switchDpid is not None) and (vlanId is not None) and (outPort is not None)):
            url = tsaBE.SDN_CONTROLLER_URI + '/stats/flowentry/add'

            actions = []
            actions.append({"type":"SET_VLAN_VID", "vlan_vid": str(vlanId)})
            actions.append({"type":"OUTPUT", "port": str(outPort)})

            match = self.getFullMatchFields(matchFields)

            if inPort is not None:
                match["in_port"] = str(inPort)

            data = {"dpid": str(int(switchDpid, 16)),
                    tsaBE.REST_PRIORITY: str(priority),
                    "match": match,
                    "actions": actions}


            printLogMsg(logging.DEBUG, "installRuleWithVlanPush", "The url is: %s, and the request content is: %s" %(url, str(data)))

            try:
                req = urllib2.Request(url)
                req.add_header('Content-Type', 'application/json')
                urllib2.urlopen(req, json.dumps(data))
            except:
                printLogMsg(logging.WARNING, "installRuleWithVlanPush", "[%s] Failed to install a rule with push vlan: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                                                              str(vlanId), str(outPort)))
                return False

            self.policyChainKeyToData[policyChainKey][2].append(switchDpid)
            return True

        printLogMsg(logging.WARNING, "installRuleWithVlanPush", "[%s] Failed to install a rule with push vlan: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                                                      str(vlanId), str(outPort)))
        return None



    def installRuleWithVlanPop(self, policyChainKey, switchDpid, vlanId, outPort, inPort=None, priority=FLOW_DEFAULT_PRIORITY):

        if ((switchDpid is not None) and (vlanId is not None) and (outPort is not None)):
            url = tsaBE.SDN_CONTROLLER_URI + '/stats/flowentry/add'

            actions = []
            actions.append({"type":"STRIP_VLAN"}) #0x8100
            actions.append({"type":"OUTPUT", "port": str(outPort)})

            match = {}
            match["in_port"] = str(inPort)
            match["dl_vlan"] = str(vlanId)

            data = {"dpid": str(int(switchDpid, 16)),
                    tsaBE.REST_PRIORITY: str(priority),
                    "match": match,
                    "actions": actions}

            printLogMsg(logging.DEBUG, "installRuleWithVlanPop", "The url is: %s, and the request content is: %s" %(url, str(data)))
            try:
                req = urllib2.Request(url)
                req.add_header('Content-Type', 'application/json')
                urllib2.urlopen(req, json.dumps(data))
            except:
                printLogMsg(logging.WARNING, "installRuleWithVlanPop", "[%s] Failed to install a rule with push vlan: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                                                             str(vlanId), str(outPort)))

                return False

            self.policyChainKeyToData[policyChainKey][2].append(switchDpid)
            return True

        printLogMsg(logging.WARNING, "installRuleWithVlanPop", "[%s] Failed to install a rule with push vlan: inPort: %s, vlanId: %s, outPort: %s" %(str(switchDpid), str(inPort),
                                                                                                                                                     str(vlanId), str(outPort)))
        return None



    def uninstallPolicyChainRules(self, policyChainKey, senderHost, matchFields):
        printLogMsg(logging.DEBUG, "uninstallPolicyChainRules", "Received arguments are: policyChainKey: %s, senderHost: %s, matchFields: %s" %(str(policyChainKey), str(senderHost), str(matchFields)))

        [fullPolicychain, vlanId, fullFlow] = self.policyChainKeyToData[policyChainKey]

        for i in range(len(fullFlow)):
            if str(fullFlow[i]).startswith("$"):
                continue

            elif i == 1:    # the first cell is the sender host name
                switchDpidToPacketsSender = self.hostsThatSendPackets[senderHost][0]
                switchPortToPacketsSender = self.hostsThatSendPackets[senderHost][1]
                match = self.getFullMatchFields(matchFields)

                if switchPortToPacketsSender is not None:
                    match["in_port"] = str(switchPortToPacketsSender)

                self.uninstallRuleForMatchfields(switchDpidToPacketsSender, matchFields)

            else:
                self.uninstallVlanRule(fullFlow[i], vlanId, -1)



    def uninstallRuleForMatchfields(self, switchDpid, matchFields):
        printLogMsg(logging.DEBUG, "uninstallRuleForMatchfields", "Received arguments are: switchDpid: %s, matchFields: %s" %(str(switchDpid), str(matchFields)))

        if ((switchDpid is not None)):

            url = tsaBE.SDN_CONTROLLER_URI + '/stats/flowentry/delete'
            match = self.getFullMatchFields(matchFields)

            data = {"dpid": str(int(switchDpid, 16)),
                    "match": match}


            printLogMsg(logging.DEBUG, "uninstallRuleForMatchfields", "The url is: %s, and the request content is: %s" %(url, str(data)))

            try:
                req = urllib2.Request(url)
                req.add_header('Content-Type', 'application/json')
                urllib2.urlopen(req, json.dumps(data))
            except:
                printLogMsg(logging.WARNING, "uninstallRuleForMatchfields", "[%s] Failed to uninstall a rule: vlmatch fieldsanId: %s." %(str(switchDpid), str(matchFields)))

        printLogMsg(logging.WARNING, "uninstallRuleForMatchfields", "[%s] Rule with match fields id: %s was uninstalled successfully." %(str(switchDpid), str(matchFields)))



    def uninstallVlanRule(self, switchDpid, vlanId, inPort):
        """
        if in_port receives -1 it means to uninstall all the rules with the received vlan id from the received switch dpid.
        """
        printLogMsg(logging.DEBUG, "uninstallVlanRule", "Received arguments are: switchDpid: %s, vlanId: %s, inPort: %s" %(str(switchDpid), str(vlanId), str(inPort)))

        if ((switchDpid is not None) and (vlanId is not None) and (inPort is not None)):

            if ((switchDpid is not None) and (vlanId is not None)):
                url = tsaBE.SDN_CONTROLLER_URI + '/stats/flowentry/delete'

                match = {}
                match["dl_vlan"] = str(vlanId)

                if inPort > 0:
                    match["in_port"] = str(inPort)

                data = {"dpid": str(int(switchDpid, 16)),
                        "match": match}

                printLogMsg(logging.DEBUG, "uninstallVlanRule", "The url is: %s, and the request content is: %s" %(url, str(data)))

                try:
                    req = urllib2.Request(url)
                    req.add_header('Content-Type', 'application/json')
                    urllib2.urlopen(req, json.dumps(data))
                except:
                    printLogMsg(logging.WARNING, "uninstallVlanRule", "[%s] Failed to uninstall a rule: vlanId: %s." %(str(switchDpid), str(vlanId)))

        printLogMsg(logging.WARNING, "uninstallVlanRule", "[%s] Rule with vlan id: %s was uninstalled successfully." %(str(switchDpid), str(vlanId)))



    def unregisterMiddlebox(self, mbName):
        printLogMsg(logging.INFO, "unregisterMiddlebox", "Received middlebox name is: %s" %mbName)

        failedPolicyChains = ""
        for policyChainKey in self.policyChainKeyToData.keys():
            try:
                [fullPolicychain, vlanId, fullFlow] = self.policyChainKeyToData[policyChainKey]


                if mbName in fullPolicychain:
                    # note that pcid will stay the same even for the new policy chain
                    pcid = self.getPcid(policyChainKey)

                    printLogMsg(logging.DEBUG, "unregisterMiddlebox", "Policychain of middleboxes is: %s" %str(fullPolicychain))

                    senderHost = fullFlow[0]
                    senderHost = senderHost[1:] # remove '$' sign

                    matchFields = self.getMatchFieldsFromKey(policyChainKey)

                    self.uninstallPolicyChainRules(policyChainKey, senderHost, matchFields)

                    # update the policy chain
                    fullPolicychain.remove(mbName)

                    newPolicyChainKey = self.generatePolicyChainKeyWithoutMiddlebox(policyChainKey, mbName)
                    printLogMsg(logging.DEBUG, "unregisterMiddlebox", "new policyChainKey is: %s" %str(newPolicyChainKey))

                    self.pcidToPolicyChainKey[pcid] = newPolicyChainKey

                    self.installPolicyChainRules(newPolicyChainKey, fullPolicychain, vlanId, matchFields, senderHost)

                    # remove the old key
                    self.policyChainKeyToData.pop(policyChainKey)

                    printLogMsg(logging.DEBUG, "unregisterMiddlebox", "new policychain data for new policyChainKey is: %s" %str(self.policyChainKeyToData[newPolicyChainKey]))

            except:
                printLogMsg(logging.DEBUG, "unregisterMiddlebox", "Caught an exception")
                failedPolicyChains += ",".join(fullPolicychain) + "$"

        if failedPolicyChains == "":
            return self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, "")
        else:
            return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to remove middlebox %s from the following policy chains: %s.' %(mbName, failedPolicyChains))



    def replaceDPIInstance(self, newDpiInstanceName, policyChainKey):
        """
        policyChain is a list of middleboxes names
        vlanId is an integer number
        """

        printLogMsg(logging.INFO, "replaceDPIInstance", "Received arguemtns are: newDpiInstanceName: %s, policyChainKey: %s" %(str(newDpiInstanceName), str(policyChainKey)))

        try:
            [fullPolicychain, vlanId, fullFlow] = self.policyChainKeyToData[policyChainKey]

            senderHost = fullFlow[0]
            senderHost = senderHost[1:] # remove '$' sign

            matchFields = self.getMatchFieldsFromKey(policyChainKey)

            # we can't uninstall part of the rules, since there may be rule that is used more than once, and we may
            # remove it on the first time.

            self.uninstallPolicyChainRules(policyChainKey, senderHost, matchFields)

            # update the policy chain
            fullPolicychain[0] = newDpiInstanceName

            self.installPolicyChainRules(policyChainKey, fullPolicychain, vlanId, matchFields, senderHost)

            printLogMsg(logging.DEBUG, "replaceDPIInstance", "new policychain data for policychainKey is: %s" %str(self.policyChainKeyToData[policyChainKey]))

            return self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, 'DPI instance was replaced successfully.')

        except:
            printLogMsg(logging.DEBUG, "replaceDPIInstance", "Caught an exception")
            return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to replace dpi instance.')



    def removePolicyChain(self, policyChainKey, shouldNotifyDpiController):
        printLogMsg(logging.INFO, "removePolicyChain", "Received arguemtns are: policyChainKey: %s, shouldNotifyDpiController: %s" %(str(policyChainKey), str(shouldNotifyDpiController)))
        [senderHost, userPolicyChain, matchFieldsAsKey] = policyChainKey.split("$")

        matchFields = self.getMatchFieldsFromKey(policyChainKey)
        printLogMsg(logging.DEBUG, "removePolicyChain", "The match field of this policychain is: %s" %str(matchFields))

        isSuccess = True

        if shouldNotifyDpiController:
            fullPolicychain = self.policyChainKeyToData[policyChainKey][0]
            preparedPolicyChain = self.preparePolicyChainToSend(fullPolicychain)
            pcid = self.getPcid(policyChainKey)

            arguments = {}
            arguments[tsaBE.POLICY_CHAIN_STRING] = preparedPolicyChain
            arguments[tsaBE.POLICY_CHAIN_ID_STRING] = str(pcid)

            request = self.toJSONRequestFormat(tsaBE.REMOVE_POLICY_CHAIN_DPI_CONT_COMMAND, arguments)

            result = self.sendToDPIController(request)

            # todo: enable it after implementing DPI Instance
            # if result[tsaBE.RETURN_VALUE_STRING].lower() == tsaBE.FAILED_STRING:
            #     isSuccess = False

        if isSuccess:
            self.uninstallPolicyChainRules(policyChainKey, senderHost, matchFields)
            self.policyChainKeyToData.pop(policyChainKey)
            self.popPcid(policyChainKey)

            return self.toJSONResponseFormat(tsaBE.SUCCESS_STRING, 'Policy chain: %s was removed successfully.' %userPolicyChain)
        else:
            return self.toJSONResponseFormat(tsaBE.FAILED_STRING, 'Failed to remove policy chain: %s.' %userPolicyChain)



    def sendToDPIController(self, data):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((self.dpiControllerIp, int(self.dpiControllerPort)))

        printLogMsg(logging.INFO, "sendToDPIController", "Send the following data to dpi controller: %s" %(str(json.dumps(data))))

        s.send(json.dumps(data) + '\n')
        res = s.recv(1024)
        result = json.loads(res)
        s.close()

        return result



    def preparePolicyChainWithHostToSend(self, policyChain):
        """
        returns a policy chain which is separated by ',' and without spaces
        """
        splittedPolicyChain = policyChain.split(",")
        for i in range(len(splittedPolicyChain)):
            splittedPolicyChain[i] = splittedPolicyChain[i].strip()

        hostSender = splittedPolicyChain.pop(0)
        return [hostSender, ",".join(splittedPolicyChain)]



    def preparePolicyChainToSend(self, policyChain):
        printLogMsg(logging.DEBUG, "preparePolicyChainToSend", "Received argument is: policyChain: %s" %(str(policyChain)))
        for i in range(len(policyChain)):
            policyChain[i] = policyChain[i].strip()

        return ",".join(policyChain)



    def extractMatchFields(self, fields):
        matchFields = []
        for match in re.findall(r"([\s]*([^=\s]+)[\s]*=[\s]*([^\s,]+)),?[\s]*", fields):
            matchFields.append((match[1], match[2]))

        return matchFields



    def toJSONRequestFormat(self, command, arguments):
        request = {}
        request[tsaBE.COMMAND_STRING] = command
        request[tsaBE.ARGUMENTS_COMMAND] = arguments

        return request



    def toJSONResponseFormat(self, status, data):
        response = {}
        response[tsaBE.RETURN_VALUE_STRING] = status
        response[tsaBE.DATA_STRING] = str(data)

        return response



    def generatePolicyChainKeyWithoutMiddlebox(self, policyChainKey, mbName):
        [senderHost, policychain, matchFields] = policyChainKey.split("$")
        policychain = policychain.split(",")
        policychain.remove(mbName)
        return senderHost + "$" + ",".join(policychain) + "$" + matchFields



    def generatePolicyChainKey(self, senderHost, preparedPolicyChain, matchFields):
        """
        A policyChain key is composed of the sender host, the middleboxes policyChain and the match fields. For example,
        if the user inserts this line: addPolicyChain h1,m8,m3 {tp_src=3334}, the policyChainKey will be:
        u'h1$m8,m3$tp_dst:3334'
        """

        printLogMsg(logging.DEBUG, "generatePolicyChainKey", "Received arguments are: senderHost: %s, preparedPolicyChain: %s, matchFields: %s" %(str(senderHost), str(preparedPolicyChain), str(matchFields)))
        key = senderHost + "$" + preparedPolicyChain + "$"

        # sort the match fields in order to ignore the order the user insert them
        matchFields.sort()

        key += matchFields[0][0] + ":" + matchFields[0][1]

        for i in range(len(matchFields)-1):
            key += "*" + matchFields[i+1][0] + ":" + matchFields[i+1][1]

        return key



    def getMatchFieldsFromKey(self, policyChainKey):
        matchFieldsAsKey = policyChainKey.split("$")[2]

        matchFields = []
        for match in re.findall(r"([^=\s:]+)[\s]*:[\s]*([^\s\*]+)", matchFieldsAsKey):
            matchFields.append((match[0], match[1]))

        return matchFields



    def getPcid(self, policyChainKey):
        """
        find the pcid matching to the received policy chain key
        """

        pcid = None
        for p in self.pcidToPolicyChainKey:
            if self.pcidToPolicyChainKey[p] == policyChainKey:
                pcid = p

        printLogMsg(logging.DEBUG, "getPcid", "The pcid of policyChainKey: %s is: %s" %(str(policyChainKey), str(pcid)))
        return pcid



    def popPcid(self, policyChainKey):
        for p in self.pcidToPolicyChainKey:
            if self.pcidToPolicyChainKey[p] == policyChainKey:
                self.pcidToPolicyChainKey.pop(p)
                printLogMsg(logging.DEBUG, "popPcid", "Pop pcid %s of policyChainKey: %s" %(str(p), str(policyChainKey)))
                return

        printLogMsg(logging.DEBUG, "popPcid", "Doesn't find pcid for policyChainKey: %s" %str(policyChainKey))




if len(sys.argv) != 3 and len(sys.argv) != 4:
    print "please use as follow: python TSA.py <dpiController Ip Address> <dpiController server port> [debug]"
    print "for example: python TSA.py 10.0.0.100 9091 debug"
else:
    debugMode = False
    for arg in sys.argv:
        if arg == "debug":
            debugMode = True

    if debugMode:
        logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
    else:
        logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

    tsa = tsaBE(sys.argv[1], sys.argv[2])