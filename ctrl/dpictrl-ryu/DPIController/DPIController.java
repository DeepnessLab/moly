package DPIController;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ConcurrentMap;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.json.*;



public class DPIController {

	private static Logger LOGGER = Logger.getLogger(DPIController.class.getName());

	// a mapping between a Middlebox name to the Middlebox id
	private ConcurrentMap <String, Middlebox> _registeredMiddleboxes;

	// a mapping between a dpi instance name to the dpi instance
	private ConcurrentMap <String, DPIInstance> _activeDpiInstances;

	// a mapping between a dpi index to dpi object
	private ConcurrentMap<Integer, DPIInstance> _dpiIndexToDPIObject;

	// create a matrix that looks like:
	// 			inst1 |	inst2 |	inst3
	// ----------------------------------
	// load  |
	// mb1Id |
	// mb2Id |
	// mb3Id |
	// mb4Id |
	//
	// while all the middleboxes in the matrix were already registered. in addition, the value of each cell
	// is a number that represents the number of policy chains that using this middlebox for this dpi instance [0..]
	// the first column is for the load (index = 0)
	private int[][] _MBsDpiInstanceMatrix;

	// this linked list holds available indexes for the dpi instance
	private ConcurrentLinkedQueue<Integer> _availableIndicesForDPIInstance;

	// this linked list holds available indexes for the middleboxes
	private ConcurrentLinkedQueue<Integer> _availableIndicesForMiddleboxes;

	private int _port;
	private int _dpiInstListeningPort;
	private int _nextSequentialMiddleboxIndex;
	private String _dpiInstNameInRemoveProcess;
	private int _nextSequentialDPIInstanceIndex;


	private static final int TIMEOUTE_MILLISECONDS_TO_WAIT_FOR_RESPONSE = 1000;
	private static final String POLICY_CHAIN_SEPERATOR = ",";
	private static final int maxInst = 30;
	private static final int maxMB = 10;



	// the commands must be in lower case !!
	private static final String REGISTER_DPI_INSTANCE_COMMAND = "registerdpiinstancecommand";
	private static final String REPLACE_DPI_INSTANCE_COMMAND = "replacedpiinstancecommand";
	private static final String REMOVE_DPI_INSTANCE_COMMAND = "removedpiinstancecommand";
	private static final String REGISTER_MIDDLEBOX_COMMAND = "registermiddleboxcommand";
	private static final String UNREGISTER_MIDDLEBOX_COMMAND = "unregistermiddleboxcommand";
	private static final String ADD_POLICY_CHAIN_COMMAND = "addpolicychaincommand";
	private static final String REMOVE_POLICY_CHAIN_COMMAND = "removepolicychaincommand";
	private static final String SET_MATCH_RULES_COMMAND = "setmatchrulescommand";
	private static final String PRINT_DPI_CONTROLLER_STATUS_COMMAND = "printdpicontrollerstatuscommand";
	private static final String PRINT_DPI_CONTROLLER_FULL_STATUS_COMMAND = "printdpicontrollerfullstatuscommand";

	// parameters for json communication
	private static final String ARGUMENTS_STRING = "arguments";
	private static final String COMMAND_STRING = "command";
	private static final String DATA_STRING = "data";
	private static final String RETURN_VALUE_STRING = "return value";
	private static final String SUCCESS_STRING = "success";
	private static final String FAILED_STRING = "failed";
	private static final String MIDDLEBOX_NAME_STRING = "middlebox name";
	private static final String MATCH_RULES_STRING = "match rules";
	private static final String FLOW_FLAG_BOOLEAN_STRING = "flow flag";
	private static final String STEALTH_FLAG_BOOLEAN_STRING = "stealth flag";
	private static final String POLICY_CHAIN_STRING = "policy chain";
	private static final String POLICY_CHAIN_ID_STRING = "pcid";
	private static final String DPI_INST_NAME = "dpi instance name";
	private static final String DPI_INST_IP = "dpi instance ip";


	private static final String TSA_IP = "10.0.0.101";
	private static final int TSA_PORT = 9093;



	public static void printLogMsg(Level level, String functionName, String msg) {
		String msgToLog = "[" + functionName + "] " + msg;

		if (level == Level.FINE)
			LOGGER.fine(msgToLog);
		else if (level == Level.INFO)
			LOGGER.info(msgToLog);
		else if (level == Level.WARNING)
			LOGGER.warning(msgToLog);
		else if (level == Level.SEVERE)
			LOGGER.severe(msgToLog);
	}



	public DPIController(int port, int dpiInstListeningPort, boolean isDebugMode) {
		_port = port;
		_dpiInstListeningPort = dpiInstListeningPort;
		_nextSequentialMiddleboxIndex = 1;
		_dpiInstNameInRemoveProcess = null;
		_nextSequentialDPIInstanceIndex = 0;
		_registeredMiddleboxes  = new ConcurrentHashMap<String, Middlebox>(maxMB);
		_activeDpiInstances = new ConcurrentHashMap<String, DPIInstance>(maxInst);
		_dpiIndexToDPIObject = new ConcurrentHashMap<Integer, DPIInstance>(maxInst);

		_MBsDpiInstanceMatrix = new int[maxMB+1][maxInst];

		_availableIndicesForDPIInstance = new ConcurrentLinkedQueue<Integer>();
		_availableIndicesForMiddleboxes = new ConcurrentLinkedQueue<Integer>();

		if (isDebugMode) {
			Handler[] handlers = Logger.getLogger( "" ).getHandlers();
			for (int index=0; index < handlers.length; index++ )
				handlers[index].setLevel( Level.FINE );

			LOGGER.setLevel(Level.FINE);
		}
		else {
			LOGGER.setLevel(Level.INFO);
		}

		printLogMsg(Level.INFO, "DPIController", "starting DPI controller..");


		Thread serverThread = new Thread(new Runnable() {
		     @Override
			public void run() {
		    	 startServer();
		     }
		});

		serverThread.start();
	}



	private void startServer() {

		ServerSocket listener;
		try {
			listener = new ServerSocket(_port);

			try {
	            while (true) {
	                Socket socket = listener.accept();
	                printLogMsg(Level.FINE, "startServer", "accept socket..");
	                try {
	                	 BufferedReader in = new BufferedReader(
	                             new InputStreamReader(socket.getInputStream()));

                         String val = in.readLine();
                         String returnedJsonMessage = executeCommand(val);

                         if (returnedJsonMessage != null) {
                        	 printLogMsg(Level.INFO, "startServer", "send this data: " + returnedJsonMessage);
                        	 PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
                        	 out.println(returnedJsonMessage);
                         }

	                } catch (Exception e) {
	                	printLogMsg(Level.WARNING, "startServer", "An error has occured");
	                	e.printStackTrace();
	                } finally {
	                    socket.close();
	                }
	            }
	        }
	        finally {
	            listener.close();
	        }
		} catch (IOException e) {
			e.printStackTrace();
		}
	}



	private String executeCommand(String msgAsString) {
		printLogMsg(Level.FINE, "executeCommand", "received the line: " + msgAsString);
		RETURN_VALUE ret = RETURN_VALUE.SUCCESS;
		String responseContent = "";
		JSONObject returnJson = null;
		boolean shouldAddNewLine = false;

		try {

			JSONObject msg = new JSONObject(msgAsString);
			String command = msg.getString(COMMAND_STRING);
			JSONObject arguments = msg.getJSONObject(ARGUMENTS_STRING);

            System.out.println("command is: " + command.toLowerCase());
            printLogMsg(Level.FINE, "executeCommand", "the received command is: " + command.toLowerCase());

			switch (command.toLowerCase()) {
				case REGISTER_DPI_INSTANCE_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'register dpi instance' command");
					String dpiInstName = arguments.getString(DPI_INST_NAME);
					String dpiInstIp = arguments.getString(DPI_INST_IP);
					returnJson = registerDPIInstance(dpiInstName, dpiInstIp);

					// add new line because it is sent to c program
					shouldAddNewLine = true;

					break;

				case REMOVE_DPI_INSTANCE_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'remove dpi instance' command");
					String dpiInsName = arguments.getString(DPI_INST_NAME);
					String policyChainsThatFiledToBeReassigned = removeDPIInstance(dpiInsName);

					if (policyChainsThatFiledToBeReassigned != "") {
						ret = RETURN_VALUE.FAILED;
						responseContent = policyChainsThatFiledToBeReassigned;
					}
					break;

				case REGISTER_MIDDLEBOX_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'register middlebox' command");
					String mbName = arguments.getString(MIDDLEBOX_NAME_STRING);
					JSONObject matchRulesJson = arguments.getJSONObject(MATCH_RULES_STRING);
					printLogMsg(Level.FINE, "executeCommand", "the received match rules are: " + matchRulesJson.toString());
					HashMap<String, HashMap<String, String>> matchRules = matchRulesToMap(matchRulesJson);

					boolean isFlow = arguments.getBoolean(FLOW_FLAG_BOOLEAN_STRING);
					boolean isStealth = arguments.getBoolean(STEALTH_FLAG_BOOLEAN_STRING);

					ret = registerMiddlebox(mbName, matchRules, isFlow, isStealth);
					break;

				case UNREGISTER_MIDDLEBOX_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'unregister middlebox' command");
					mbName = arguments.getString(MIDDLEBOX_NAME_STRING);
					String dpiInstanceNamesThatFailed = unregisterMiddlebox(mbName);

					if (dpiInstanceNamesThatFailed != "") {
						ret = RETURN_VALUE.FAILED;
						responseContent = dpiInstanceNamesThatFailed;
					}
					break;

				case ADD_POLICY_CHAIN_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'add policy chain' command");
					String policyChainToAdd = arguments.getString(POLICY_CHAIN_STRING);
					String pcid = arguments.getString(POLICY_CHAIN_ID_STRING);
					String newPolicyChain = addPolicyChain(policyChainToAdd, pcid);

					if (newPolicyChain == null) {
						ret = RETURN_VALUE.FAILED;
						responseContent = "failed to add policy chain: " + policyChainToAdd;
					} else {
						responseContent = newPolicyChain;
					}

					break;

				case REMOVE_POLICY_CHAIN_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'remove policychain' command");
					String policyChainToRemove = arguments.getString(POLICY_CHAIN_STRING);
					pcid = arguments.getString(POLICY_CHAIN_ID_STRING);
					ret = removePolicyChain(policyChainToRemove, pcid);
					break;

				case SET_MATCH_RULES_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'set match rules' command");
					mbName = arguments.getString(MIDDLEBOX_NAME_STRING);
					matchRulesJson = arguments.getJSONObject(MATCH_RULES_STRING);
					printLogMsg(Level.INFO, "executeCommand", "the received match rules are: " + matchRulesJson.toString());

					matchRules = matchRulesToMap(matchRulesJson);

					dpiInstanceNamesThatFailed = setMatchRules(mbName, matchRules);

					if (dpiInstanceNamesThatFailed != "") {
						ret = RETURN_VALUE.FAILED;

						responseContent = dpiInstanceNamesThatFailed;
					}

					break;

				case PRINT_DPI_CONTROLLER_STATUS_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'print dpi controller status' command");
					printStatus();
					break;
				case PRINT_DPI_CONTROLLER_FULL_STATUS_COMMAND:
					printLogMsg(Level.INFO, "executeCommand", "processing 'print dpi controller full status' command");
					printFullStatus();
					break;
				default:
					printLogMsg(Level.WARNING, "executeCommand", "invalid command");
					ret = RETURN_VALUE.FAILED;
					responseContent = "Invalid command.";
					break;

			}
		} catch (JSONException e) {
			printLogMsg(Level.WARNING, "executeCommand", "invalid json message format");
			e.printStackTrace();
		}


		if (returnJson == null)
			returnJson = toJsonResponseFormat(ret, responseContent);

		if (shouldAddNewLine)
			return returnJson.toString() + "\n";

		return returnJson.toString();
	}



	private void printStatus() {
		printLogMsg(Level.INFO, "printStatus", "----------------------------------------------------------------------------------------");
		printLogMsg(Level.INFO, "printStatus", "the registered middleboxes are:");

		for (String key : _registeredMiddleboxes.keySet()) {
		    System.out.println(_registeredMiddleboxes.get(key).toString());
		}

		printLogMsg(Level.INFO, "printStatus", "----------------------------------------------------------------------------------------");

		printLogMsg(Level.INFO, "printStatus", "the active dpi instances are:");
		for (String name : _activeDpiInstances.keySet()) {
			System.out.println(_activeDpiInstances.get(name).toString());
		}

		printLogMsg(Level.INFO, "printStatus", "----------------------------------------------------------------------------------------");

		printLogMsg(Level.INFO, "printStatus", "the mapping between middleboxes ids and policy chains are:");

		for (int i=0; i<maxMB + 1; i++) {
			for (int j=0; j<maxInst; j++)
				System.out.print(_MBsDpiInstanceMatrix[i][j] + " ");
			System.out.println();
		}
	}



	private void printFullStatus() {
		printLogMsg(Level.INFO, "printFullStatus", "----------------------------------------------------------------------------------------");
		printLogMsg(Level.INFO, "printFullStatus", "the registered middleboxes are:");

		for (String key : _registeredMiddleboxes.keySet()) {
		    _registeredMiddleboxes.get(key).printFullStatus();
		}

		printLogMsg(Level.INFO, "printFullStatus", "----------------------------------------------------------------------------------------");

		printLogMsg(Level.INFO, "printFullStatus", "the active dpi instances are:");
		for (String name : _activeDpiInstances.keySet()) {
			System.out.println(_activeDpiInstances.get(name).toString());
		}

		printLogMsg(Level.INFO, "printFullStatus", "----------------------------------------------------------------------------------------");

		printLogMsg(Level.INFO, "printFullStatus", "the mapping between middleboxes ids and policy chains are:");

		for (int i=0; i<maxMB + 1; i++) {
			for (int j=0; j<maxInst; j++)
				System.out.print(_MBsDpiInstanceMatrix[i][j] + " ");
			System.out.println();
		}
	}



	private RETURN_VALUE registerMiddlebox(String mbName, HashMap<String, HashMap<String, String>> matchRules, boolean isFlow, boolean isStealth) {
		printLogMsg(Level.FINE, "registerMiddlebox", "Received arguments are: mbName: " + mbName + ", matchRules: " + matchRules + ", isFlow: " + isFlow + ", isStealth: " + isStealth);
		if (_registeredMiddleboxes.containsKey(mbName))
			return RETURN_VALUE.ALREADY_EXIST;

		RETURN_VALUE ret = RETURN_VALUE.SUCCESS;

		int mbId = generateMiddleboxIndex();

		_registeredMiddleboxes.put(mbName, new Middlebox(mbName, mbId, matchRules, isFlow, isStealth));

		return ret;
	}


	private String unregisterMiddlebox(String mbName) {
		printLogMsg(Level.FINE, "unregisterMiddlebox", "Received argument is: mbName: " + mbName);

		if (!_registeredMiddleboxes.containsKey(mbName))
			return "";


		Middlebox mb = _registeredMiddleboxes.remove(mbName);

		int mbIndex = mb.getIndex();

		_availableIndicesForMiddleboxes.add(mbIndex);
		String dpiInstanceNamesThatFailed = "";


		for (int i=0; i<maxInst; i++) {
			if (_MBsDpiInstanceMatrix[mbIndex][i] > 0) {

				String dpiInstName = _dpiIndexToDPIObject.get(i).getName();

				if (!unregisterMiddleboxFromDPIInstance(dpiInstName, mbName))
					dpiInstanceNamesThatFailed += dpiInstName + ", ";

				// reset the cell, so after this function ends, all the row related to this
				// middlebox will be 0.
				_MBsDpiInstanceMatrix[mbIndex][i] = 0;
			}
		}

		sendUnregisterMiddleboxCommandToTsa(mbName);

		return dpiInstanceNamesThatFailed;
	}



	// returns true if the un-registration has completed successfully, false otherwise.
	private boolean unregisterMiddleboxFromDPIInstance(String dpiInstName, String mbName) {
		String retVal = RETURN_VALUE.FAILED.toString();

		try {
			JSONObject request = toJsonRequestFormat(UNREGISTER_MIDDLEBOX_COMMAND, mbName);
			String response = sendToDPIInstance(dpiInstName, request, true);

			JSONObject jsonResponse = new JSONObject(response);
			retVal = jsonResponse.getString(RETURN_VALUE_STRING);

		} catch (JSONException e) {}

		if (retVal.equals(RETURN_VALUE.FAILED.toString())) {
			return false;
		}

		return true;
	}



	private void sendUnregisterMiddleboxCommandToTsa(String mbName) {
		// send the command to the TSA
		String msgToTsa = toJsonRequestFormat(UNREGISTER_MIDDLEBOX_COMMAND, mbName).toString();

		Socket socket = null;
		try {
			socket = new Socket(TSA_IP, TSA_PORT);
			PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
			out.println(msgToTsa);

			socket.close();

		} catch (Exception e) {
			e.printStackTrace();
		}
	}



	private String addPolicyChain(String policyChain, String pcid) {
		printLogMsg(Level.FINE, "addPolicyChain", "Received arguments are: policyChain: " + policyChain + ", pcid: " + pcid);

		// assign dpi instance for this policychain
		String dpiInstanceName = aggregate(policyChain);

		if (dpiInstanceName == null) {
			return null;
		}
		else {
			DPIInstance dpiInst = _activeDpiInstances.get(dpiInstanceName);

			// the policy chain before adding the dpi instance
			dpiInst.addPolicyChain(policyChain, pcid);

			updateDPIInstanceCol(policyChain, dpiInstanceName, "add");

			sendMatchRulesToDPIInstance(dpiInstanceName, policyChain);
		}

		printLogMsg(Level.FINE, "addPolicyChain", "Add dpi instance " + dpiInstanceName + " to the received policy chain");
		String newPolicyChain = addDPIInstanceToPolicyChain(policyChain, dpiInstanceName);
		return newPolicyChain;
	}



	// send all the patterns sets of middleboxes shown in the received policy chain to the received dpi instance
	// the json that is sent looks like: {COMMAND_STRING: SET_PATTERNS_SET_COMMAND, "<middleboxName1>": "<patternSet>", "<middleboxName2>": "<patternSet>", ...}
	private void sendMatchRulesToDPIInstance(String dpiInstanceName, String policyChain) {

		DPIInstance instanceObj = _activeDpiInstances.get(dpiInstanceName);

		Socket socket = null;

		if (instanceObj == null)
			return;

		String[] splitted = getMBNames(policyChain);
		JSONObject data = new JSONObject();

		try {
			for (int i=0; i<splitted.length; i++) {
				data.put(splitted[i], mapToMatchRules(_registeredMiddleboxes.get(splitted[i]).getMatchRules()));
			}


			String jsonMsg = toJsonRequestFormat(SET_MATCH_RULES_COMMAND, data).toString();
			printLogMsg(Level.FINE, "sendMatchRulesToDPIInstance", "The json message that is sent to the DPI instance is: " + jsonMsg.toString());

			// TODO: enable it after developing DPIInstance.py !!!!!!
//			socket = new Socket(instanceObj.getIp(), _dpiInstListeningPort);
//			PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
//			out.println(jsonMsg);
//
//			socket.close();

		} catch (Exception e) {
			printLogMsg(Level.WARNING, "sendMatchRulesToDPIInstance", "Failed to send match rules to DPI instance: " + dpiInstanceName);
			e.printStackTrace();
		}
	}



	private String aggregate(String policyChain) {
		String dpiInstName = null;
		int minNumOfHandledMiddleboxes = Integer.MAX_VALUE;

		for(ConcurrentHashMap.Entry<String, DPIInstance> entry : _activeDpiInstances.entrySet()){
		    if (entry.getValue().getNumOfHandledMiddleboxes() < minNumOfHandledMiddleboxes &&
		    		!entry.getKey().equals(_dpiInstNameInRemoveProcess)) {

		    	dpiInstName = entry.getKey();
		    	minNumOfHandledMiddleboxes = entry.getValue().getNumOfHandledMiddleboxes();
		    }
		}

		printLogMsg(Level.INFO, "aggregate", "The assigned dpi instance is: " + dpiInstName);
		return dpiInstName;
	}



	private JSONObject registerDPIInstance(String dpiInstName, String dpiInstIp) {

		// if the dpi instance is already exists (but register again), we send to it all
		// the match rules it should take care of.
		if (_activeDpiInstances.containsKey(dpiInstName)) {
			LinkedList<Pair<String, String>> policyChains = _activeDpiInstances.get(dpiInstName).getAllPoliciesChains();
			Iterator<Pair<String, String>> iter = policyChains.iterator();

			while (iter.hasNext()) {
				String policyChain = iter.next().getFirst();
				sendMatchRulesToDPIInstance(dpiInstName, policyChain);
			}

			return toJsonResponseFormat(RETURN_VALUE.SUCCESS, "");
		}


		int dpiIndex = generateDPIInstanceIndex();

		if (dpiIndex == -1) {
			printLogMsg(Level.SEVERE, "registerDPIInstance", "Can't register the DPI Instance. Total number of instances exceed the maximal number (" + maxInst + ").");

			JSONObject response = toJsonResponseFormat(RETURN_VALUE.FAILED, "Can't register the DPI Instance. Total number of instances exceed the maximal number (" + maxInst + ").");
			return response;
		}

		DPIInstance newInst = new DPIInstance(dpiInstName, dpiIndex, dpiInstIp);

		_activeDpiInstances.put(dpiInstName, newInst);
		_dpiIndexToDPIObject.put(dpiIndex, newInst);

		return toJsonResponseFormat(RETURN_VALUE.SUCCESS, "");
	}



	private synchronized String removeDPIInstance(String dpiInstName) {

		// for addPolicyChain usage
		_dpiInstNameInRemoveProcess = dpiInstName;

		int dpiIndex = _activeDpiInstances.get(dpiInstName).getIndex();
		_availableIndicesForDPIInstance.add(dpiIndex);
		_dpiIndexToDPIObject.remove(dpiIndex);
		DPIInstance instanceObject = _activeDpiInstances.remove(dpiInstName);

		String failedPolicyChains = "";

		if (instanceObject != null) {
			LinkedList<Pair<String, String>> policiesChains = instanceObject.getAllPoliciesChains();

			Iterator<Pair<String, String>> iter = policiesChains.iterator();

			// iterate over all the policy chains of the removed dpi instance, and assign
			// new dpi instance for that policy chain.
			while (iter.hasNext()) {
				Pair<String, String> policyChain = iter.next();
				printLogMsg(Level.FINE, "removeDPIInstance", "policychain is: " + policyChain.getFirst());

				String newPolicyChain = addPolicyChain(policyChain.getFirst(), policyChain.getSecond());

				if (newPolicyChain == null) {

					failedPolicyChains += policyChain;
				}
				else {
					String newInstName = getDPIInstanceFromPolicyChain(newPolicyChain);

					if (!sendReplaceDPIInstanceToTsa(newInstName, policyChain.getSecond()))
						failedPolicyChains += policyChain;
				}
			}
		}

		// reset the dpi instance column
		for (int i=0; i< maxMB+1; i++)
			_MBsDpiInstanceMatrix[i][dpiIndex] = 0;

		_dpiInstNameInRemoveProcess = null;
		return failedPolicyChains;
	}



	// returns true if the replacement was completed successfully, otherwise - false
	private boolean sendReplaceDPIInstanceToTsa(String newInstName, String pcid) {
		// send the command to the tsa
		String msgToTsa = toJsonRequestFormat(REPLACE_DPI_INSTANCE_COMMAND, newInstName + " " + pcid).toString();

		Socket socket = null;
		try {
			socket = new Socket(TSA_IP, TSA_PORT);
			PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
			out.println(msgToTsa);

			BufferedReader in = new BufferedReader(
                    new InputStreamReader(socket.getInputStream()));
			String msgAsString = in.readLine();
            System.out.println("msgAsString: " + msgAsString);
            printLogMsg(Level.FINE, "removeDPIInstance", "the message that is received from the tsa is: " + msgAsString);

            socket.close();

            JSONObject msg = new JSONObject(msgAsString);
			String retVal = msg.getString(RETURN_VALUE_STRING);


			if (!SUCCESS_STRING.equals(retVal)) {
				return false;
			}

		} catch (Exception e) {
			e.printStackTrace();
		}

		return true;
	}




	private String[] getMBNames (String policyChain) {
		return policyChain.split(POLICY_CHAIN_SEPERATOR);
	}



	private int[] getMBIds (String policyChain) {
		String[] splittedMBNames = policyChain.split(POLICY_CHAIN_SEPERATOR);

		int[] mbIds = new int[splittedMBNames.length];

		for (int i=0; i< splittedMBNames.length; i++)
			if (_registeredMiddleboxes.get(splittedMBNames[i]) != null)
				mbIds[i] = _registeredMiddleboxes.get(splittedMBNames[i]).getIndex();
			else {
				mbIds[i] = -1;
				printLogMsg(Level.WARNING, "getMBIds", "the middlebox: " + splittedMBNames[i] + ", is not registered.");
			}

		return mbIds;
	}



	private void updateDPIInstanceCol (String policyChain, String dpiInstanceName, String command) {

		int dpiInstanceIndex = _activeDpiInstances.get(dpiInstanceName).getIndex();

		int[] mbIds = getMBIds(policyChain);

		// add/remove one to all the middleboxes that take part in the received policy chain
		if (command.equals("add")) {
			// we start from 1 since i=0 is saved for load
			for (int i=0; i<mbIds.length; i++) {

				//
				if (mbIds[i] > -1)
					_MBsDpiInstanceMatrix[mbIds[i]][dpiInstanceIndex]++;
			}
		}
		else if (command.equals("remove")) {
			// we start from 1 since i=0 is saved for load
			for (int i=0; i<mbIds.length; i++) {
				// remove only if the middlebox exist (it get -1 if the middlebox shown in
				// the policy chain is not registered.
				if (mbIds[i] > -1) {
					int val = _MBsDpiInstanceMatrix[mbIds[i]][dpiInstanceIndex];

					if (val > 0)
						_MBsDpiInstanceMatrix[mbIds[i]][dpiInstanceIndex] = val-1;
				}
			}
		}

	}



	// the function receives the policy chain and the assigned dpi instance, and return a new
	// policy chain which the dpi instance is part of the policy chain
	private String addDPIInstanceToPolicyChain (String policyChain, String dpiInstanceName) {
		return dpiInstanceName + POLICY_CHAIN_SEPERATOR + policyChain;
	}



	private String getDPIInstanceFromPolicyChain (String policyChain) {
		String[] splitted = policyChain.split(POLICY_CHAIN_SEPERATOR);

		System.out.println("~getDPIInstanceFromPolicyChain , return: " + splitted[0]);
		printLogMsg(Level.FINE, "getDPIInstanceFromPolicyChain", "the dpi instance of policychain: " + policyChain + ", is: " + splitted[0]);
		return splitted[0];
	}



	private String sendToDPIInstance (String dpiInstName, JSONObject msg, boolean waitToResponse) {
		printLogMsg(Level.FINE, "sendToDPIInstance", "send to " + dpiInstName + " the following message: " + msg.toString());

		DPIInstance instanceObj = _activeDpiInstances.get(dpiInstName);
		String response = "";
		Socket socket = null;

		if (instanceObj == null)
			return null;

		try {
			// TODO enable it after developing DPIInstance.py !!!!!!!!
//			socket = new Socket(instanceObj.getIp(), _dpiInstListeningPort);
//			PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
//			out.println(msg.toString());
//			BufferedReader input = new BufferedReader(new InputStreamReader(socket.getInputStream()));
//
//			if (waitToResponse) {
//				socket.setSoTimeout(TIMEOUTE_MILLISECONDS_TO_WAIT_FOR_RESPONSE);
//				response = input.readLine();
//			}
//
//			socket.close();

		} catch (Exception e) {
			printLogMsg(Level.WARNING, "sendToDPIInstance", "Failed to send the message: " + msg.toString() + ", to dpi instance: " + dpiInstName);
		}

		printLogMsg(Level.FINE, "sendToDPIInstance", "the response from the dpi instance is: " + response.toString());

		return response;
	}



	private String instanceNameExtraction (String policyChain) {

		String[] splitted = getMBNames(policyChain);

		if (splitted != null && splitted.length > 0) {
			printLogMsg(Level.FINE, "instanceNameExtraction", "the dpi instance name is: " + splitted[0]);
			return splitted[0];
		} else {
			return null;
		}
	}



	private RETURN_VALUE removePolicyChain(String policyChain, String pcid) {

		String dpiInstName = instanceNameExtraction(policyChain);
		String retVal = sendRemovePolicyCommandToDPIInstance(policyChain, dpiInstName);

		// TODO: replace with this line after implementing DPI Instance support
//		if (SUCCESS_STRING.equals(retVal)) {
		if (true) {
			updateDPIInstanceCol(policyChain, dpiInstName, "remove");

			if (pcid != null) {
				Iterator<Pair<String, String>> iter = _activeDpiInstances.get(dpiInstName).getAllPoliciesChains().iterator();

				while (iter.hasNext()) {
					Pair<String, String> pair = iter.next();

					if (pcid.equals(pair.getSecond()))
						iter.remove();
				}

				return RETURN_VALUE.SUCCESS;
			}
			printLogMsg(Level.WARNING, "updateDPIInstanceCol", "pcid of policyChain: " + policyChain + ", and dpi instance name: " + dpiInstName + ", is null.");
		}

		return RETURN_VALUE.FAILED;
	}



	private String sendRemovePolicyCommandToDPIInstance(String policyChain, String dpiInstName) {
		try {
			JSONObject arguments = new JSONObject();
			arguments.put(POLICY_CHAIN_STRING, policyChain);
			JSONObject request = toJsonRequestFormat(REMOVE_POLICY_CHAIN_COMMAND, arguments);
			String response = sendToDPIInstance(dpiInstName, request, true);


			JSONObject jsonResponse = new JSONObject(response);
			return jsonResponse.getString(RETURN_VALUE_STRING);

		} catch (JSONException e) {
			printLogMsg(Level.WARNING, "sendRemovePolicyCommandToDPIInstance", "Failed to send 'remove policy chain' command to DPI instance.");
			e.printStackTrace();
			return FAILED_STRING;
		}
	}



	// returns the next available index number, or -1 in case of an error.
	private synchronized int generateDPIInstanceIndex() {
		if (_availableIndicesForDPIInstance.isEmpty())
			// verify that the new instance dosn't exceed the maximal number of instances
			if (_nextSequentialDPIInstanceIndex == maxInst - 1)
				return -1;
			else
				return _nextSequentialDPIInstanceIndex++;

		return _availableIndicesForDPIInstance.remove();
	}



	// returns the next available index number, or -1 in case of an error.
	private synchronized int generateMiddleboxIndex() {
		if (_availableIndicesForMiddleboxes.isEmpty())
			// verify that the new instance dosn't exceed the maximal number of instances
			if (_nextSequentialMiddleboxIndex== maxInst - 1)
				return -1;
			else
				return _nextSequentialMiddleboxIndex++;

		return _availableIndicesForMiddleboxes.remove();
	}



	private String setMatchRules(String mbName, HashMap<String, HashMap<String, String>> newMatchRules) {

		Middlebox mb = _registeredMiddleboxes.get(mbName);
		printLogMsg(Level.FINE, "setMatchRules", "set match rules of '" + mbName +"' from: '" + mb.getMatchRules() + "' to: '" + newMatchRules + "'");

		mb.setMatchRules(newMatchRules);

		int mbIndex = mb.getIndex();

		String dpiInstanceNamesThatFailed = "";

		for (int i=0; i<maxInst; i++) {
			if (_MBsDpiInstanceMatrix[mbIndex][i] > 0) {

				String dpiInstName = _dpiIndexToDPIObject.get(i).getName();

				if (!sendSetMatchRulesCommandToDPIInstance(mb, newMatchRules, dpiInstName)) {
					dpiInstanceNamesThatFailed += dpiInstName + ", ";
				}
			}
		}

		return dpiInstanceNamesThatFailed;
	}



	// returns true if the operation completed successfully, false otherwise.
	private boolean sendSetMatchRulesCommandToDPIInstance(Middlebox mb, HashMap<String, HashMap<String, String>> newMatchRules, String dpiInstName) {
		JSONObject arguments = new JSONObject();
		String retVal = RETURN_VALUE.FAILED.toString();

		try {
			arguments.put(mb.getName(), new JSONObject(newMatchRules));
			JSONObject request = toJsonRequestFormat(SET_MATCH_RULES_COMMAND, arguments);
			String response = sendToDPIInstance(dpiInstName, request, true);

			JSONObject jsonResponse = new JSONObject(response);
			retVal = jsonResponse.getString(RETURN_VALUE_STRING);

		} catch (JSONException e) {}

		if (retVal.equals(RETURN_VALUE.FAILED.toString())) {
			return false;
		}

		return true;
	}



	private JSONObject toJsonRequestFormat(String command, JSONObject arguments) {
		JSONObject jsonObj = new JSONObject();
		try {
			jsonObj.put(COMMAND_STRING, command);
			jsonObj.put(ARGUMENTS_STRING, arguments.toString());
		} catch (JSONException e) {
			e.printStackTrace();
		}

		return jsonObj;
	}



	private JSONObject toJsonRequestFormat(String command, String argument) {
		JSONObject jsonObj = new JSONObject();
		try {
			jsonObj.put(COMMAND_STRING, command);
			jsonObj.put(ARGUMENTS_STRING, argument);
		} catch (JSONException e) {
			e.printStackTrace();
		}

		return jsonObj;
	}



	private JSONObject toJsonResponseFormat(RETURN_VALUE retVal, String content) {
		JSONObject jsonObj = new JSONObject();
		try {
			jsonObj.put(RETURN_VALUE_STRING, retVal.toString());
			jsonObj.put(DATA_STRING, content);
		} catch (JSONException e) {
			e.printStackTrace();
		}

		return jsonObj;
	}



	private static HashMap<String, HashMap<String, String>> matchRulesToMap(JSONObject jObject) throws JSONException {

        HashMap<String, HashMap<String, String>> matchRules = new HashMap<>();

        Iterator<?> keys = jObject.keys();

        while(keys.hasNext()) {
            String ruleId = (String)keys.next();
            HashMap<String, String> ruleParams = new HashMap<String, String>();
            JSONObject ruleObjcet = jObject.getJSONObject(ruleId);

            Iterator<?> ruleParamsIter = ruleObjcet.keys();

            while (ruleParamsIter.hasNext()) {
            	String ruleParamName = (String)ruleParamsIter.next();
            	String ruleParamValue = ruleObjcet.getString(ruleParamName);
            	ruleParams.put(ruleParamName, ruleParamValue);
            }

            matchRules.put(ruleId, ruleParams);
        }

        printLogMsg(Level.FINE, "matchRulesToMap", "the match rules as a map are: " + matchRules);
        return matchRules;
    }



	private static JSONObject mapToMatchRules(HashMap<String, HashMap<String, String>> map) throws JSONException {

        JSONObject matchRules = new JSONObject();
        Iterator<String> rulesIter = map.keySet().iterator();

        while (rulesIter.hasNext()) {
        	String ruleId = rulesIter.next();
        	HashMap<String, String> ruleData = map.get(ruleId);

        	matchRules.put(ruleId, new JSONObject(ruleData));
        }

        printLogMsg(Level.FINE, "matchRulesToMap", "the match rules as a json object are: " + matchRules.toString());
        return matchRules;
    }
}
