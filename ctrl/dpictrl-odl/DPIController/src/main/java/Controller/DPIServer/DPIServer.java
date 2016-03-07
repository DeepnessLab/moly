package Controller.DPIServer;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.apache.log4j.Logger;

import Common.Middlebox;
import Common.ServiceInstance;
import Common.Protocol.ControllerMessage;
import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.InstanceDeregister;
import Common.Protocol.InstanceRegister;
import Common.Protocol.MatchRule;
import Common.Protocol.MiddleboxDeregister;
import Common.Protocol.MiddleboxRegister;
import Common.Protocol.MiddleboxRulesetAdd;
import Common.Protocol.MiddleboxRulesetRemove;
import Common.Protocol.RuleAdd;
import Common.Protocol.RuleRemove;
import Controller.ConfigManager;
import Controller.DPIController;
import Controller.InternalMatchRule;

/**
 * handle the communiction with the different instances, dispatch message and
 * passes to controller Created by Lior on 25/11/2014.
 */
public class DPIServer implements IDPIServiceFacade {
	// TODO: extract middlebox facade
	private static final Logger LOGGER = Logger.getLogger(DPIServer.class);
	private final DPIController _controller;
	private final int _port;
	private final boolean _listening;
	private final List<DPIServerThread> _idleThreads;
	private final Map<Middlebox, DPIServerThread> _middleboxThreads;
	private final Map<ServiceInstance, DPIServerThread> _servicesThreads;

	// TODO: add common parent\encapsulation to middlebox and service instance
	public DPIServer(DPIController dpiController) {
		_middleboxThreads = new HashMap<Middlebox, DPIServerThread>();
		_servicesThreads = new HashMap<ServiceInstance, DPIServerThread>();
		_controller = dpiController;
		_idleThreads = new LinkedList<>();
		_port = Short.parseShort(ConfigManager
				.getProperty("DPIServer.listeningPort"));
		_listening = true;
	}

	/**
	 * waits for incoming connections and rules changes
	 * 
	 * @param dpiController
	 */
	public void run() {
		try {
			ServerSocket serverSocket = new ServerSocket(_port);
			LOGGER.info("Dpi controller is up!");
			while (_listening) {
				Socket clientSocket = serverSocket.accept();
				DPIServerThread serverThread = new DPIServerThread(
						clientSocket, _controller, this);
				_idleThreads.add(serverThread);
				serverThread.start();
			}
			serverSocket.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	@Override
	public boolean sendMessage(ServiceInstance instance, ControllerMessage msg) {
		DPIServerThread thread = _servicesThreads.get(instance);
		try {
			thread.sendMessage(msg);
		} catch (IOException e) {
			LOGGER.error("cant send message to client: " + instance);
			e.printStackTrace();
			return false;
		}
		return true;
	}

	private void dispacthMessage(DPIServerThread thread, MiddleboxRegister msg) {
		Middlebox mb = new Middlebox(msg.id, msg.name);
		mb.address = thread.getClientAddress();
		_middleboxThreads.put(mb, thread);
		_idleThreads.remove(thread);
		_controller.registerMiddlebox(mb);

	}

	private void dispacthMessage(DPIServerThread thread, MiddleboxDeregister msg) {
		Middlebox mb = new Middlebox(msg.id, msg.name);
		thread.setKeepRunning(false);
		_middleboxThreads.remove(mb);
		_controller.deregisterMiddlebox(mb);
	}

	private void dispacthMessage(DPIServerThread thread, MiddleboxRulesetAdd msg) {
		_controller.addRules(new Middlebox(msg.id), msg.rules);
	}

	private void dispacthMessage(DPIServerThread thread,
			MiddleboxRulesetRemove msg) {
		_controller.removeRules(new Middlebox(msg.id), msg.rules);
	}

	private void dispacthMessage(DPIServerThread thread, InstanceRegister msg) {
		ServiceInstance instance = new ServiceInstance(msg.id, msg.name);
		instance.address = thread.getClientAddress();
		_servicesThreads.put(instance, thread);
		_idleThreads.remove(thread);
		_controller.registerInstance(instance);
	}

	private void dispacthMessage(DPIServerThread thread, InstanceDeregister msg) {
		ServiceInstance instance = new ServiceInstance(msg.id);
		thread.setKeepRunning(false);
		_servicesThreads.remove(instance);
		_controller.deregisterInstance(instance);
	}

	@Override
	public void deallocateRule(List<InternalMatchRule> rules,
			ServiceInstance instance) {
		LOGGER.debug(String.format("removing rules: %s from instance %s",
				rules, instance));
		RuleRemove ruleRemove = new RuleRemove();
		List<Integer> rids = new LinkedList<>();
		for (MatchRule rule : rules) {
			rids.add(rule.rid);
		}
		ruleRemove.rules = rids;
		sendMessage(instance, ruleRemove);
	}

	@Override
	public void assignRules(List<InternalMatchRule> rules,
			ServiceInstance instance) {
		RuleAdd ruleAdd = new RuleAdd();
		ruleAdd.rules = new LinkedList<MatchRule>(rules);
		sendMessage(instance, ruleAdd);
	}

	public void dispacthMessage(DPIServerThread thread, DPIProtocolMessage msg) {
		if (msg instanceof InstanceDeregister)
			this.dispacthMessage(thread, (InstanceDeregister) msg);
		else if (msg instanceof InstanceRegister)
			this.dispacthMessage(thread, (InstanceRegister) msg);
		else if (msg instanceof MiddleboxDeregister)
			this.dispacthMessage(thread, (MiddleboxDeregister) msg);
		else if (msg instanceof MiddleboxRegister)
			this.dispacthMessage(thread, (MiddleboxRegister) msg);
		else if (msg instanceof MiddleboxRulesetAdd)
			this.dispacthMessage(thread, (MiddleboxRulesetAdd) msg);
		else if (msg instanceof MiddleboxRulesetRemove)
			this.dispacthMessage(thread, (MiddleboxRulesetRemove) msg);
	}

}
