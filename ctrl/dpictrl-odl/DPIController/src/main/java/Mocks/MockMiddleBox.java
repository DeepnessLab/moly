package Mocks;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Scanner;

import org.apache.log4j.Logger;
import org.apache.log4j.MDC;

import Common.JsonUtils;
import Common.Protocol.MatchRule;
import Common.Protocol.MiddleboxDeregister;
import Common.Protocol.MiddleboxMessage;
import Common.Protocol.MiddleboxMessageFactory;
import Common.Protocol.MiddleboxRegister;
import Common.Protocol.MiddleboxRulesetAdd;
import Common.Protocol.MiddleboxRulesetRemove;
import Controller.DPIController;

/**
 * Created by Lior on 12/11/2014.
 */
public class MockMiddleBox extends Thread {

	private final MiddleboxMessageFactory _messageFactory;
	private final InetAddress _controllerIp;
	private final int _controllerPort;
	private final String _id;
	private final String _name;
	private Socket _socket;
	private boolean _waitForInput = true;
	private PrintWriter _sendOut = null;
	private final String USAGE = "exit|add-rules rid,pattern[,regex] ...|remove-rules rid1,rid2,..\n"
			+ "add-rules-file <rules-filename> [max_rules]| remove-rules-file <rules-filename> [max_rules]";
	private boolean _interactice = true;
	private static final Logger LOGGER = Logger.getLogger(MockMiddleBox.class);

	public MockMiddleBox(InetAddress controllerIp, int controllerPort,
			String id, String name) {

		_controllerIp = controllerIp;
		_controllerPort = controllerPort;
		_id = id;
		_name = name;
		_messageFactory = new MiddleboxMessageFactory(id, name);
		MDC.put("type", "Middlebox");
		MDC.put("id", _id);
	}

	/**
	 * wait for action from ui add/remove rules
	 */
	@Override
	public void run() {
		sendDerigesterOnClosed();
		try {
			LOGGER.info(String.format("starting middlebox id=%s,name=%s ", _id,
					_name));
			LOGGER.info(String.format("connecting to controller: %s(%d) ",
					_controllerIp, _controllerPort));
			_socket = new Socket(_controllerIp.getHostAddress(),
					_controllerPort);
			_sendOut = new PrintWriter(_socket.getOutputStream(), true);
			MiddleboxRegister msg = _messageFactory.createRegistration();
			LOGGER.debug("Sending registeration..");
			sendMessageToController(msg);
			LOGGER.debug("waiting for user input..");
			if (isInteractice()) {
				waitForInput();
			} else {
				waitForConnectionClose();
			}
		} catch (Exception e) {
			LOGGER.error(e.getMessage());
		}
		LOGGER.info(String.format("Middlebox %s stopped", _id));
	}

	private void waitForConnectionClose() throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				_socket.getInputStream()));
		String inputLine;
		while ((inputLine = in.readLine()) != null) {
		}
	}

	private void sendDerigesterOnClosed() {
		Runtime.getRuntime().addShutdownHook(new Thread() {
			@Override
			public void run() {
				MDC.put("type", "Middlebox");
				MDC.put("id", _id);
				LOGGER.debug("sending Deregister message to controller");
				MiddleboxDeregister msg = _messageFactory
						.createDeregistration();
				sendMessageToController(msg);
			}
		});
	}

	private void sendMessageToController(MiddleboxMessage msg) {
		String registerMsg = JsonUtils.toJson(msg);
		_sendOut.println(registerMsg);
	}

	private void waitForInput() throws IOException {
		// BufferedReader br = new BufferedReader(new
		// InputStreamReader(System.in));
		Scanner sc = new Scanner(System.in);
		while (_waitForInput) {
			System.out.println("Enter command:");
			String command = sc.nextLine();
			String[] commandArgs = command.split(" ");
			boolean isValid;
			switch (commandArgs[0]) {
			case "exit":
				isValid = handleExitCommand(commandArgs);
				break;
			case "add-rules":
				isValid = handleAddRuleCommand(commandArgs);
				break;
			case "remove-rules":
				isValid = handleRemoveRulesCommand(commandArgs);
				break;
			case "add-rules-file":
				isValid = handleAddRulesFileCommand(commandArgs);
				break;
			case "remove-rules-file":
				isValid = handleRemoveRulesFileCommand(commandArgs);
				break;
			default:
				isValid = false;
			}
			if (!isValid) {
				System.out.println("Unknown command");
				System.out.println(USAGE);
			}
		}
		LOGGER.info("Adios!");
	}

	private boolean handleRemoveRulesFileCommand(String[] commandArgs) {
		if (commandArgs.length != 2) {
			return false;
		}
		int maxRules = -1;
		if (commandArgs.length == 3) {
			maxRules = Integer.valueOf(commandArgs[2]);
		}
		List<MatchRule> rules;
		try {
			rules = JsonUtils.parseRulesFile(commandArgs[1], maxRules);
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
		List<Integer> ruleIds = new LinkedList<Integer>();
		for (MatchRule matchRule : rules) {
			ruleIds.add(matchRule.rid);
		}
		MiddleboxRulesetRemove msg = _messageFactory
				.createRulesetRemove(ruleIds);
		this.sendMessageToController(msg);
		return true;
	}

	private boolean handleAddRulesFileCommand(String[] commandArgs) {
		if (commandArgs.length < 2 || commandArgs.length > 3) {
			return false;
		}
		int maxRules = -1;
		if (commandArgs.length == 3) {
			maxRules = Integer.valueOf(commandArgs[2]);
		}
		String filePath = commandArgs[1];
		return loadRulesFile(filePath, maxRules);
	}

	/**
	 * load a MatchRules file in a json format
	 * 
	 * @param maxRules
	 *            only the first maxRules rules will be loaded, -1 loads all
	 *            rules
	 * @param filePath
	 * @return true if all rules loaded successfully
	 */
	public boolean loadRulesFile(String filePath, int maxRules) {
		List<MatchRule> rules;
		LOGGER.info(String.format(
				"loading %d match-rules from %s (-1 is all rules)", maxRules,
				filePath));
		try {
			rules = JsonUtils.parseRulesFile(filePath, maxRules);
			LOGGER.info(rules.size() + " match-rules loaded!");
		} catch (IOException e) {
			LOGGER.error(e.getMessage());
			return false;
		}

		MiddleboxRulesetAdd msg = _messageFactory.createRulesetAdd(rules);
		sendMessageToController(msg);
		return true;
	}

	private boolean handleRemoveRulesCommand(String[] commandArgs) {
		if (commandArgs.length != 2)
			return false;
		String rulesString = commandArgs[1];
		List<String> rules = Arrays.asList(rulesString.split(","));

		List<Integer> ruleIds = new LinkedList<Integer>();
		for (String rule : rules) {
			ruleIds.add(Integer.valueOf(rule));
		}

		MiddleboxRulesetRemove msg = _messageFactory
				.createRulesetRemove(ruleIds);
		this.sendMessageToController(msg);
		return true;
	}

	private boolean handleAddRuleCommand(String[] commandArgs) {
		if (commandArgs.length < 2)
			return false;
		List<MatchRule> rules = new ArrayList<MatchRule>();
		for (int i = 1; i < commandArgs.length; i++) {
			MatchRule rule = parseRule(commandArgs[i]);
			if (rule == null)
				return false;
			rules.add(rule);
		}
		MiddleboxRulesetAdd msg = _messageFactory.createRulesetAdd(rules);
		sendMessageToController(msg);
		return true;
	}

	private MatchRule parseRule(String ruleArg) {
		String[] ruleParams = ruleArg.split(",");
		if (ruleParams.length != 2 && ruleParams.length != 3)
			return null;
		MatchRule rule = new MatchRule(Integer.valueOf(ruleParams[0]));
		rule.pattern = ruleParams[1];
		if (ruleParams.length == 3) {
			if (ruleParams[2].equals("regex")) {
				rule.is_regex = true;
			} else
				return null;
		} else {
			rule.is_regex = false;
		}
		return rule;
	}

	private boolean handleExitCommand(String[] commandArgs) {
		MiddleboxDeregister msg = _messageFactory.createDeregistration();
		sendMessageToController(msg);
		_waitForInput = false;
		return true;
	}

	public boolean isInteractice() {
		return _interactice;
	}

	/**
	 * does the middlebox will run with user interface or not
	 * 
	 * @param interactice
	 */
	public void setInteractice(boolean interactice) {
		this._interactice = interactice;
	}
}
