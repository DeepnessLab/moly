package wrappers;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

import org.apache.log4j.Logger;
import org.apache.log4j.MDC;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.ParameterException;

import Common.JsonUtils;
import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.InstanceDeregister;
import Common.Protocol.InstanceMessage;
import Common.Protocol.InstanceMessageFactory;
import Common.Protocol.InstanceRegister;
import Common.Protocol.MatchRule;
import Common.Protocol.RuleAdd;
import Common.Protocol.RuleRemove;
import Controller.DPIController;
import Mocks.ListenerMockThread;

public class DPIServiceWrapper extends Thread {

	private final InetAddress _controllerIp;
	private final int _controllerPort;
	private final String _id;
	private final String _name;
	private final InstanceMessageFactory _messageFactory;
	private Socket _socket;
	private PrintWriter _sendOut;
	private final HashMap<Integer, MatchRule> _rules;
	private final String RULES_SUFFIX = "rules.json";
	private final String INTERFACE;
	private final ExecutableWrapper _processHandler;
	private static final Logger LOGGER = Logger
			.getLogger(DPIServiceWrapper.class);

	public DPIServiceWrapper(InetAddress controllerIp, int controllerPort,
			String id, String name) throws FileNotFoundException, IOException {
		MDC.put("type", "instance");
		MDC.put("id", id);
		_controllerIp = controllerIp;
		_controllerPort = controllerPort;
		_id = id;
		_name = name;
		INTERFACE = name + "-eth0";
		_messageFactory = new InstanceMessageFactory(id, name);
		_rules = new HashMap<>();
		_processHandler = new ExecutableWrapper("/moly_service",
				"dpi_service.exe");

	}

	public void run() {
		Runtime.getRuntime().addShutdownHook(new Thread() {
			@Override
			public void run() {
				MDC.put("type", "instance");
				MDC.put("id", _id);
				InstanceDeregister msg = _messageFactory.createDeregistration();
				sendMessageToController(msg);
				_processHandler.stopProcess();
			}
		});
		try {
			_socket = new Socket(_controllerIp.getHostAddress(),
					_controllerPort);
			_sendOut = new PrintWriter(_socket.getOutputStream(), true);
			LOGGER.info(String.format("dpi service %s:%s is up!", _id, _name));
			InstanceRegister msg = _messageFactory.createRegistration();
			sendMessageToController(msg);
			waitForInstructions();
		} catch (IOException e) {
			LOGGER.error(e.getMessage());
		}

	}

	private void sendMessageToController(InstanceMessage msg) {
		String msgStr = JsonUtils.toJson(msg);
		LOGGER.debug("Sending : " + msgStr);
		_sendOut.println(msgStr);
	}

	/**
	 * waits and prints every message received by controller
	 * 
	 * @throws IOException
	 */
	private void waitForInstructions() throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				_socket.getInputStream()));
		String inputLine;
		while ((inputLine = in.readLine()) != null) {
			LOGGER.trace("got: "
					+ (inputLine.length() < 200 ? inputLine : (inputLine
							.substring(0, 200) + "(truncated)")));
			DPIProtocolMessage controllerMsg = JsonUtils.fromJson(inputLine);
			handleMessage(controllerMsg);

		}
	}

	private void handleMessage(DPIProtocolMessage controllerMsg) {
		if (controllerMsg instanceof RuleAdd) {
			addRules(((RuleAdd) controllerMsg).rules);
		} else if (controllerMsg instanceof RuleRemove) {
			removeRules(((RuleRemove) controllerMsg).rules);
		} else {
			LOGGER.warn("Unknown message from controller:\n" + controllerMsg);
			return;
		}
	}

	private void reloadService() {
		String rulesFile = "./" + _name + RULES_SUFFIX;
		writeRulesToFile(_rules.values(), rulesFile);
		try {
			LinkedList<String> args = new LinkedList<String>();
			args.add("rules=" + rulesFile);
			args.add("in=" + INTERFACE);
			args.add("out=" + INTERFACE);
			args.add("max=" + _rules.size());
			_processHandler.runProcess(args);
		} catch (IOException e) {
			LOGGER.error(e.getMessage());
		}
		LOGGER.info(String.format("reloading DPI service with %d rules!",
				_rules.size()));
	}

	private void writeRulesToFile(Collection<MatchRule> rules, String rulesFile) {
		try {
			PrintWriter outFile = new PrintWriter(new FileWriter(rulesFile));

			for (MatchRule matchRule : rules) {
				String matchStr = matchRule.toString();
				outFile.println(matchStr);
			}
			outFile.close();
		} catch (IOException e) {
			LOGGER.error(e.getMessage());
		}

	}

	private void removeRules(List<Integer> rules) {
		boolean isRulesChanged = false;
		int i = 0;
		for (Integer ruleId : rules) {
			MatchRule removed = _rules.remove(ruleId);
			if (removed == null) {
				LOGGER.warn(String.format(
						"ruleID %s not exists, shouldnt happen!", ruleId));
			} else {
				isRulesChanged = true;
				i++;
			}
		}
		LOGGER.info(String.format("%d rules has been removed", i));
		if (isRulesChanged) {
			reloadService();
		}
	}

	private void addRules(List<MatchRule> rules) {
		boolean isRulesChanged = false;
		int i = 0;
		for (MatchRule matchRule : rules) {
			if (_rules.containsKey(matchRule.rid)) {
				LOGGER.warn(String.format("ruleid %s already exists",
						matchRule.rid));

			} else {
				i++;
				_rules.put(matchRule.rid, matchRule);
				isRulesChanged = true;
			}
		}
		LOGGER.info(String.format("%d rules has been added", i));
		if (isRulesChanged) {
			reloadService();
		}
	}

	public static void main(String[] args) throws FileNotFoundException,
			IOException {

		DPIServiceWrapperArgs params = new DPIServiceWrapperArgs();
		JCommander argsParser = new JCommander(params);
		try {
			argsParser.parse(args);
		} catch (ParameterException e) {
			LOGGER.error(e.getMessage());
			argsParser.usage();
			return;
		}

		try {
			InetAddress controllerIp = Inet4Address
					.getByName(params.controller);
			new DPIServiceWrapper(controllerIp, params.controllerPort,
					params.id, params.getName()).start();
			if (params.printPackets) {
				ListenerMockThread.startPrintingIncomingPackets(params.bpf,
						params.getInInterface());
			}
		} catch (UnknownHostException e) {
			LOGGER.error(params.controller + " is an invalid Ip address");
			argsParser.usage();
		} catch (NumberFormatException e) {
			argsParser.usage();
		}

	}

}
