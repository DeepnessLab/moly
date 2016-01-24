__author__ = 'orenstal'

from threading import Thread
import sys
import json
import socket
import commands
import re
import logging

import scapy.config
scapy.config.conf.use_pcap = True

try:
    import scapy.all as scapy
except:
    import scapy as scapy


def printLogMsg(level, functionName, msg):
    msgToLog = '[' + functionName + '] ' + msg

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


class middlebox(object):

    # commands between the middlebox and the tsa BE
    REGISTER_MIDDLEBOX_COMMAND = "registermiddleboxcommand"
    SET_MATCH_RULES_COMMAND = "setmatchrulescommand"
    UNREGISTER_COMMAND = "unregistermiddleboxcommand"

    # commands between the middlebox and the tsa BE
    RELOAD_MATCH_RULES_CLI = "reloadmatchrules"
    EXIT_CLI = "exit"

    # parameters for json communication
    COMMAND_STRING = "command"
    ARGUMENTS_COMMAND = "arguments"
    RETURN_VALUE_STRING = "return value"
    DATA_STRING = "data"
    SUCCESS_STRING = "success"
    ALREADY_EXIST_STRING = "already_exist"
    MIDDLEBOX_NAME_STRING = "middlebox name"
    MATCH_RULES_STRING = "match rules"
    FLOW_FLAG_BOOLEAN_STRING = "flow flag"
    STEALTH_FLAG_BOOLEAN_STRING = "stealth flag"


    def __init__(self, hostId, macAddress, matchRulesFilePath, dpiControllerIp, dpiControllerPort):
        self. hostId = hostId
        self.macAddress = macAddress
        self.matchRulesFilePath = matchRulesFilePath
        self.dpiControllerIp = dpiControllerIp
        self.dpiControllerPort = dpiControllerPort
        self.matchRules = self.readMatchRulesFromFile()
        self.registerToDpiController()
        self.sniffer = middleboxSniffer(hostId, macAddress)

        if __name__ == "__main__":
            self.snifferThread = Thread(target = self.sniffer.start)
            self.snifferThread.setDaemon(True)
            self.snifferThread.start()
        else:
            sys.exit()

        self.startCLI()


    def readMatchRulesFromFile(self):
        matchRules = {}
        rulesCounter = 0
        with open(self.matchRulesFilePath) as f:
            regex = re.compile(r"([\s,]+)([\w|_]+)")
            for line in f:
                if line is None or len(line) <= 2:
                    continue

                try:
                    jsonFormatLine = line.replace("\'", '"')
                    jsonFormatLine = regex.sub("%s\"%s\"" % (r"\1", r"\2"), jsonFormatLine)
                    ruleData = json.loads(jsonFormatLine)
                    matchRules[ruleData["rid"]] = {"is_regex": ruleData["is_regex"], "pattern": ruleData["pattern"]}
                    rulesCounter += 1
                except:
                    printLogMsg(logging.WARNING, "readMatchRulesFromFile", "Failed to read the following match rule: %s" %line)

        printLogMsg(logging.INFO, "readMatchRulesFromFile", "%d rules were loaded successfully." % rulesCounter)
        return matchRules



    def toJSONRequestFormat(self, command, arguments):
        request = {}
        request[middlebox.COMMAND_STRING] = command
        request[middlebox.ARGUMENTS_COMMAND] = arguments

        return request



    def registerToDpiController(self):
        arguments = {}
        arguments[middlebox.MIDDLEBOX_NAME_STRING] = ('m'+self.hostId)
        arguments[middlebox.MATCH_RULES_STRING] = self.matchRules
        arguments[middlebox.FLOW_FLAG_BOOLEAN_STRING] = False
        arguments[middlebox.STEALTH_FLAG_BOOLEAN_STRING] = False

        request = self.toJSONRequestFormat(middlebox.REGISTER_MIDDLEBOX_COMMAND, arguments)

        result = self.sendToDPIController(request)

        if result[middlebox.RETURN_VALUE_STRING].lower() == middlebox.SUCCESS_STRING:
            printLogMsg(logging.INFO, "registerToDpiController", "successfully registered the middlebox.")
        elif result[middlebox.RETURN_VALUE_STRING].lower() == middlebox.ALREADY_EXIST_STRING:
            printLogMsg(logging.INFO, "registerToDpiController", "middlebox is already registered. If you want to reload match rules, use reloadMatchRules command.")



    def reloadMatchRules(self, pathToMatchRulesFile):
        self.matchRulesFilePath = pathToMatchRulesFile
        try:
            newMatchRules = self.readMatchRulesFromFile()
        except:
            printLogMsg(logging.ERROR, "reloadMatchRules", "Failed to reload match rules.")
            return

        arguments = {}
        arguments[middlebox.MIDDLEBOX_NAME_STRING] = ('m'+self.hostId)
        arguments[middlebox.MATCH_RULES_STRING] = newMatchRules

        request = self.toJSONRequestFormat(middlebox.SET_MATCH_RULES_COMMAND, arguments)
        result = self.sendToDPIController(request)

        if result[middlebox.RETURN_VALUE_STRING].lower() == middlebox.SUCCESS_STRING:
            self.matchRules = newMatchRules
        else:
            failedInstances = result[middlebox.DATA_STRING]
            printLogMsg(logging.WARNING, "reloadMatchRules", "Failed to update the match rules on the following instances: %s." %str(failedInstances))



    def unregister(self):
        arguments = {}
        arguments[middlebox.MIDDLEBOX_NAME_STRING] = ('m'+self.hostId)

        request = self.toJSONRequestFormat(middlebox.UNREGISTER_COMMAND, arguments)
        result = self.sendToDPIController(request)

        if result[middlebox.RETURN_VALUE_STRING].lower() == middlebox.SUCCESS_STRING:
            printLogMsg(logging.INFO, "unregister", "Unregister has done successfully.")
        else:
            failedInstances = result[middlebox.DATA_STRING]
            printLogMsg(logging.WARNING, "unregister", "Exit, but failed to unregister on the following instances: %s." %str(failedInstances))



    def sendToDPIController(self, data):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((self.dpiControllerIp, int(self.dpiControllerPort)))
        printLogMsg(logging.INFO, "sendToDPIController", "Send the following data to dpi controller: %s" %(str(json.dumps(data))))

        s.send(json.dumps(data) + '\n')
        res = s.recv(1024)
        result = json.loads(res)
        s.close()

        return result


    def startCLI(self):
        menuMsg =  "\nPlease enter one of the following options:\n"
        menuMsg += "reloadMatchRules <path to match rules json file - optional>\n"
        menuMsg += "exit (unregister on the DPI Controller and then exit)\n"
        menuMsg += "------------------------------\n"

        while True:
            userInput = raw_input(menuMsg).lower()

            if userInput is None or userInput == "":
                continue

            splitted = userInput.split()

            if splitted[0] == middlebox.RELOAD_MATCH_RULES_CLI:
                if len(splitted) == 2:
                    matchRulesFilePath = splitted[1]
                    self.reloadMatchRules(matchRulesFilePath)

                else:
                    self.reloadMatchRules(self.matchRulesFilePath)

            elif splitted[0] == middlebox.EXIT_CLI:
                self.unregister()
                break

            # todo - this part tests the 'unregister dpi instance' and should not be part of the middlebox !
            # the only reason its here is to test the whole flow in the dpi controller point of view.
            elif splitted[0] == "instance":
                arguments = {}
                arguments['dpi instance name'] = 'm' + splitted[1]

                request = self.toJSONRequestFormat('removedpiinstancecommand', arguments)
                result = self.sendToDPIController(request)

                if result[middlebox.RETURN_VALUE_STRING].lower() == middlebox.SUCCESS_STRING:
                    print "removing has done successfully."
                else:
                    failedInstances = result[middlebox.DATA_STRING]
                    print 'failed to remove the following policy chains: %s.' %str(failedInstances)

            else:
                printLogMsg(logging.WARNING, "startCLI", "Illegal command")

        # it kills also the sniffer thread
        sys.exit()



MINSIZE = 0
class middleboxSniffer(object):
    def __init__(self, hostId, macAddress):
        self. hostId = hostId
        self.macAddress = macAddress


    def start(self):
        printLogMsg(logging.INFO, "start", "start new sniffering for hostId: %s on mac address: %s" %(str(self.hostId), str(self.macAddress)))

        while True:
            try:
                scapy.sniff(store=0, filter="tcp", prn=self.checkPacketType, count=1)
            except ValueError:
                pass



    def addVlanToPacket(self, pkt, vlanValue):
        """
        This function push the received vlan tag to the received packet
        """

        # gets the first layer of the current packet
        layer = pkt.firstlayer()

        # loop over the layers
        while not isinstance(layer, scapy.NoPayload):
            if 'chksum' in layer.default_fields:
                del layer.chksum

            if (type(layer) is scapy.Ether):
                # adjust ether type
                layer.type = 33024  # 0x8100

                # add 802.1q layer between Ether and IP
                dot1q = scapy.Dot1Q(vlan=vlanValue)
                dot1q.add_payload(layer.payload)
                layer.remove_payload()
                layer.add_payload(dot1q)
                layer = dot1q

            # advance to the next layer
            layer = layer.payload



    def checkPacketType(self, pkt):

       if scapy.Ether in pkt:

            if str(pkt[scapy.Ether].type) == '33024': # 0x8100
                if (str(pkt[scapy.Ether].dst) != str(self.macAddress)):
                    printLogMsg(logging.INFO, "checkPacketType", "received packet with vlan: %s" % str(pkt[scapy.Dot1Q].vlan))

                    # send the received packet only if this host doesn't send it
                    if (str(pkt[scapy.Ether].src) != str(self.macAddress)):
                        printLogMsg(logging.INFO, "checkPacketType", "forward it..")
                        scapy.sendp(pkt)

                else:
                    printLogMsg(logging.INFO, "checkPacketType", "received *my* packet with vlan %s !!" % str(pkt[scapy.Dot1Q].vlan))

            elif str(pkt[scapy.Ether].type) == '2048' and str(pkt[scapy.Ether].dst) != '00:00:00:00:00:00' and str(pkt[scapy.Ether].dst) != 'ff:ff:ff:ff:ff:ff' : # 0x800
                if (str(pkt[scapy.Ether].dst) != str(self.macAddress) and str(pkt[scapy.Ether].src) != str(self.macAddress)):
                    printLogMsg(logging.INFO, "checkPacketType", "forward it..")
                    scapy.sendp(pkt)





def getHostIdAndMac():
    parsedHostId = None
    parsedMac = None

    ifconfig = commands.getoutput("ifconfig | grep HWaddr")

    hostId = re.search('h([\d]+)-eth0.+', ifconfig)
    if hostId is not None:
        parsedHostId = hostId.group(1)

    mac = re.search('.+-eth0.+([\w]{2}:[\w]{2}:[\w]{2}:[\w]{2}:[\w]{2}:[\w]{2}).*', ifconfig)
    if mac is not None:
        parsedMac = mac.group(1)

    return [parsedHostId, parsedMac]



dpiControllerIp = None
portToDpiController = None
matchRulesFilePath = "SnortPatternsPartial.json"

if len(sys.argv) == 2:
    if sys.argv[1] == "auto":
        dpiControllerIp = "10.0.0.7"
        portToDpiController = "9091"
elif len(sys.argv) == 3:
    dpiControllerIp = sys.argv[1]
    portToDpiController = sys.argv[2]

elif len(sys.argv) == 4:
    dpiControllerIp = sys.argv[1]
    portToDpiController = sys.argv[2]
    matchRulesFilePath = sys.argv[3]

else:
    printLogMsg(logging.INFO, "main", "please use as follow: python middlebox.py <dpiController Ip Address> <dpiController server port> <match rules file path - optional>\nfor example: python middlebox.py 10.0.0.5 9091 Snort.json")
    exit(1)


[hostId, mac] = getHostIdAndMac()

if hostId is None or mac is None:
    printLogMsg(logging.CRITICAL, "main", "failed to find mac address and / or host id of interface 'eth0'")
    exit(1)

middlebox(hostId, mac, matchRulesFilePath, dpiControllerIp, portToDpiController)