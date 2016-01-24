__author__ = 'orenstal'
import socket
import json
import sys
import logging


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



class tsaFE(object):

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

    # parameters for the tsaBE server
    LISTENING_IP = '10.0.0.101'
    TSA_BE_LISTENING_PORT = 9093

    def __init__(self):
        self.startCommandLine()



    def sendToTsaBE(self, data):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((tsaFE.LISTENING_IP, tsaFE.TSA_BE_LISTENING_PORT))

        s.send(json.dumps(data) + '\n')
        res = s.recv(1024)
        result = json.loads(res)
        s.close()

        return result



    def toJSONRequestFormat(self, command, arguments):
        request = {}
        request[tsaFE.COMMAND_STRING] = command
        request[tsaFE.ARGUMENTS_COMMAND] = arguments

        return request



    def startCommandLine(self):
        menuMsg =  "\nPlease enter one of the following options:\n"
        menuMsg += "addPolicyChain <policyChain, including the first host> {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=}\n"
        menuMsg += "removePolicyChain <policyChain, including the first host> {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=}\n"
        menuMsg += "printDpiController\n"
        menuMsg += "printDpiControllerFull\n"
        menuMsg += "exit\n"
        menuMsg += "------------------------------\n"

        while True:
            userInput = raw_input(menuMsg).lower()

            if userInput is None or userInput == "":
                continue

            splitted = userInput.split()
            command = splitted[0]
            splitted.remove(command)
            arguments = ' '.join(splitted)

            if command == tsaFE.ADD_POLICY_CHAIN_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'add policychain' command")
                request = self.toJSONRequestFormat(command, arguments)
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.DATA_STRING]))

            elif command == tsaFE.REMOVE_POLICY_CHAIN_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'remove policychain' command")
                request = self.toJSONRequestFormat(command, arguments)
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.DATA_STRING]))

            elif command == tsaFE.PRINT_DPI_CONTROLLER_STATUS_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'print dpi controller status' command")
                request = self.toJSONRequestFormat(command, {})
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.RETURN_VALUE_STRING]))

            elif command == tsaFE.PRINT_DPI_CONTROLLER_FULL_STATUS_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'print dpi controller full status' command")
                request = self.toJSONRequestFormat(command, {})
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.RETURN_VALUE_STRING]))

            elif command == tsaFE.EXIT_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'exit' command")
                request = self.toJSONRequestFormat(command, {})
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.RETURN_VALUE_STRING]))
                break

            else:
                printLogMsg(logging.WARNING, "startCommandLine", "Illegal command")


debugMode = False
if len(sys.argv) == 2 and sys.argv[1] == "debug":
    debugMode = True

if debugMode:
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
else:
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

tsaFE()