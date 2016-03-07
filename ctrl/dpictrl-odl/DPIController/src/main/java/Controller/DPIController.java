package Controller;

import java.util.LinkedList;
import java.util.List;

import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.Produces;
import javax.ws.rs.core.MediaType;

import org.apache.log4j.Logger;
import org.apache.log4j.MDC;
import org.glassfish.jersey.server.JSONP;

import Common.Middlebox;
import Common.ServiceInstance;
import Common.Protocol.MatchRule;
import Controller.DPIForeman.DPIForeman;
import Controller.DPIForeman.IDPIServiceFormen;
import Controller.DPIForeman.ILoadBalanceStrategy;
import Controller.DPIForeman.MinChainsPerInstanceStrategy;
import Controller.DPIServer.DPIServer;
import Controller.MatchRuleRepository.IMatchRuleRepository;
import Controller.MatchRuleRepository.MatchRulesRepository;
import Controller.TSA.IPolicyChainsBuilder;
import Controller.TSA.ITSAFacade;
import Controller.TSA.MinInstancesPerChainBuilder;
import Controller.TSA.TSAFacadeImpl;

/**
 * This class is the DPIcontroller main class, the rules of this class: 1.
 * handle input rules from the middlebox - using ControllerThread 2. keep track
 * of all the match rules (patterns) - using the MatchRulesRepository 3. updates
 * the dpi-services on the Match Rules 4. load balance the dpi-services 5.
 * altering the policy chain of the sdn controller, making each packet
 * corresponding to one of the registered middleboxes to go through the relevant
 * service Created by Lior on 12/11/2014.
 */

public class DPIController {

	private static final Logger LOGGER = Logger.getLogger(DPIController.class);
	private IDPIServiceFormen _foreman; // handles work between instances
	private final IMatchRuleRepository _middleboxes; // rules per middlebox
	private final DPIServer _server; // handle the connections with middlebox
										// and services
	private final ITSAFacade _tsa;
	private final IPolicyChainsBuilder _chainsBuilder;

	/**
	 * @param port
	 *            on which port the controller is listening to messages
	 * @param loadBalanceStrategy
	 */
	public DPIController() {
		MDC.put("type", "Controller");
		_server = new DPIServer(this);
		_middleboxes = new MatchRulesRepository();
		ILoadBalanceStrategy strategy = new MinChainsPerInstanceStrategy(
				_middleboxes);
		_chainsBuilder = new MinInstancesPerChainBuilder(
				(MinChainsPerInstanceStrategy) strategy);
		_foreman = new DPIForeman(_server);
		_foreman.setStrategy(strategy);
		_tsa = new TSAFacadeImpl(this);
	}

	/**
	 * @return the foreman
	 */
	public IDPIServiceFormen getForeman() {
		return _foreman;
	}

	/**
	 * @param _foreman
	 *            the foreman to set
	 */
	public void setForeman(IDPIServiceFormen foreman) {
		this._foreman = foreman;
	}

	public void registerMiddlebox(Middlebox mb) {
		if (!_middleboxes.addMiddlebox(mb)) {
			LOGGER.warn("middlebox already exists: " + mb.id);
			return;
		}
		LOGGER.info("middlebox added: " + mb.name);
		_tsa.refreshPolicyChains();
	}

	public void deregisterMiddlebox(Middlebox mb) {
		LOGGER.trace(String
				.format("Middlebox %s is going to be removed", mb.id));
		List<InternalMatchRule> internalRules = _middleboxes
				.removeMiddlebox(mb);
		if (internalRules == null) {
			LOGGER.warn(String.format("no such middlebox %s", mb.id));
			return;
		}
		LOGGER.info("Removed Middlebox: " + mb.id);
		_foreman.removeJobs(internalRules, mb);
		_tsa.refreshPolicyChains();
	}

	public void removeRules(Middlebox mb, List<Integer> ruleIds) {
		List<MatchRule> rules = _middleboxes.getMatchRules(mb, ruleIds);
		List<InternalMatchRule> removedRules = _middleboxes.removeRules(mb,
				rules);
		if (removedRules == null) {
			LOGGER.warn("no such mb: " + mb.id);
			return;
		}
		LOGGER.info("going to remove: " + removedRules);
		_foreman.removeJobs(removedRules, mb);
		this.updateTSA();
	}

	public void addRules(Middlebox mb, List<MatchRule> rules) {
		List<InternalMatchRule> internalRules = _middleboxes
				.addRules(mb, rules);
		if (internalRules == null) {
			LOGGER.warn(String.format("no such middlebox %s", mb.id));
			return;
		}
		if (_foreman.addJobs(internalRules, mb)) {
			this.updateTSA();
		} else {
			LOGGER.warn("foreman cannot allocate instance for rules");
		}
		LOGGER.info(internalRules.size() + " rules added from middelbox " + mb);
	}

	public void registerInstance(ServiceInstance instance) {
		if (!_foreman.addWorker(instance)) {
			LOGGER.warn("instance already registered: " + instance);
		}
		LOGGER.info("instance added: " + instance.name);
		this.updateTSA();
	}

	public void deregisterInstance(ServiceInstance instance) {
		_foreman.removeWorker(instance);
		LOGGER.info("instance removed: " + instance.name);
		this.updateTSA();
	}

	public void run() {
		_server.run();
		updatePolicyChains(_tsa.getPolicyChains());
	}

	public void updatePolicyChains(List<PolicyChain> policyChains) {
		_foreman.setPolicyChains(policyChains);
		this.updateTSA();
	}

	/**
	 * updates the traffic steering application on the order all packets should
	 * traverse in the future: add order between middlebox , and traffic class
	 * (ie. dst port 80) -> (configuration)
	 */
	private void updateTSA() {
		List<PolicyChain> currentChains = _tsa.getPolicyChains();
		List<PolicyChain> newChains = _chainsBuilder
				.addDPIInstancesToChains(currentChains);
		if (!newChains.equals(currentChains)) {
			LOGGER.info("going to update policy chain to: "
					+ newChains.toString());
			_tsa.updatePolicyChains(newChains);
		}

	}

	public List<Middlebox> getAllMiddleBoxes() {
		return new LinkedList<Middlebox>(_middleboxes.getAllMiddleboxes());
	}

	public List<ServiceInstance> getAllInstances() {
		return new LinkedList<ServiceInstance>(_foreman.getAllInstnaces());
	}

	public List<ServiceInstance> getNeededInstances(Middlebox mb) {

		return _foreman.getNeededInstances(_middleboxes.getMatchRules(mb));
	}

	public void close() {
		// TODO: close sockets, remove instances from tsa
	}
}
