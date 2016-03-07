package Controller.DPIForeman;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import org.apache.log4j.Logger;

import Common.IChainNode;
import Common.Middlebox;
import Common.ServiceInstance;
import Controller.InternalMatchRule;
import Controller.PolicyChain;
import Controller.MatchRuleRepository.IMatchRuleRepository;

public class MinChainsPerInstanceStrategy implements ILoadBalanceStrategy {

	private List<PolicyChain> _policyChains;
	private IDPIServiceFormen _foreman;
	private Map<ServiceInstance, List<PolicyChain>> _instanceChains;
	private Map<String, ServiceInstance> _chainInstance;
	private static final Logger LOGGER = Logger
			.getLogger(MinChainsPerInstanceStrategy.class);
	private final IMatchRuleRepository _matchRules;

	public MinChainsPerInstanceStrategy(IMatchRuleRepository matchRules) {
		this._matchRules = matchRules;
		_instanceChains = new HashMap<ServiceInstance, List<PolicyChain>>();
		_chainInstance = new HashMap<String, ServiceInstance>();
	}

	@Override
	public void instanceAdded(ServiceInstance instance) {
		if (_instanceChains.containsKey(instance)) {
			LOGGER.warn("instance exists: " + instance);
			return;
		}
		if (isLoadBalanceOptimal()) {
			LOGGER.info("new instance not needed, optimal balance");
			return;
		}

		balanceChains();
	}

	/**
	 * return true if each chain has one instance or no chains exists, false
	 * otherwise
	 * 
	 * @return
	 */
	private boolean isLoadBalanceOptimal() {
		if (_policyChains == null || _policyChains.size() == 0)
			return true;
		if (_instanceChains.isEmpty())
			return false;
		for (List<PolicyChain> chains : _instanceChains.values()) {
			if (chains.size() > 1)
				return false;
		}
		return true;
	}

	private void balanceChains() {
		Collection<ServiceInstance> allInstnaces = _foreman.getAllInstnaces();
		if (_policyChains == null || _policyChains.size() == 0
				|| allInstnaces.isEmpty()) {
			return;
		}
		_foreman.deallocateRule(_foreman.getAllRules());
		_instanceChains = new HashMap<ServiceInstance, List<PolicyChain>>();
		_chainInstance = new HashMap<String, ServiceInstance>();
		List<PolicyChain> allChains = new LinkedList<PolicyChain>(_policyChains);
		Stack<ServiceInstance> instancesStack = new Stack<ServiceInstance>();
		instancesStack.addAll(allInstnaces);
		int instancesNeeded = Math.min(allChains.size(), instancesStack.size());

		int chainsPerInstance = _policyChains.size() / instancesNeeded;

		List<ServiceInstance> usedInstances = new LinkedList<ServiceInstance>();
		for (int i = 0; i < instancesNeeded; i++) {
			ServiceInstance instance = instancesStack.pop();
			List<PolicyChain> instanceChains = new LinkedList<PolicyChain>(
					allChains.subList(0, chainsPerInstance));
			allChains.subList(0, chainsPerInstance).clear();
			assignChainsToInstnace(instanceChains, instance);
			moveRulesToInstance(instance, getMatchRules(instanceChains));
			usedInstances.add(instance);
		}
		// handle the reminder
		for (int i = 0; i < allChains.size(); i++) {
			ServiceInstance instance = usedInstances.get(i);
			List<PolicyChain> instanceChains = allChains.subList(i, i + 1);
			assignChainsToInstnace(instanceChains, instance);
			moveRulesToInstance(instance, getMatchRules(instanceChains));
		}
		LOGGER.debug("after action: " + this);
	}

	@Override
	public String toString() {
		return "MinChainsPerInstanceStrategy [_instanceChains="
				+ _instanceChains + "]";
	}

	private void assignChainsToInstnace(List<PolicyChain> instanceChains,
			ServiceInstance instance) {
		if (_instanceChains.get(instance) == null)
			_instanceChains.put(instance, new LinkedList<PolicyChain>());
		_instanceChains.get(instance).addAll(instanceChains);
		for (PolicyChain policyChain : instanceChains) {
			_chainInstance.put(policyChain.trafficClass, instance);
		}

	}

	private void moveRulesToInstance(ServiceInstance instance,
			List<InternalMatchRule> rules) {
		HashSet<InternalMatchRule> distinctRules = new HashSet<InternalMatchRule>(
				rules);
		if (distinctRules.size() > 0) {
			_foreman.assignRules(new LinkedList<>(distinctRules), instance);
		}
	}

	private List<InternalMatchRule> getMatchRules(
			List<PolicyChain> instanceChains) {
		List<InternalMatchRule> result = new LinkedList<InternalMatchRule>();
		for (PolicyChain policyChain : instanceChains) {
			for (IChainNode node : policyChain.chain) {
				if (node instanceof Middlebox)
					result.addAll(_matchRules.getMatchRules((Middlebox) node));
			}
		}
		return result;
	}

	private boolean isNoInstances() {
		return _instanceChains.keySet().isEmpty();
	}

	@Override
	public void instanceRemoved(ServiceInstance instance,
			List<InternalMatchRule> instanceRules) {
		List<PolicyChain> chains = _instanceChains.remove(instance);
		if (chains == null) {
			LOGGER.warn("instance is not in use: " + instance.id);
			return;
		}
		for (PolicyChain policyChain : chains) {
			_chainInstance.remove(policyChain.trafficClass);
		}
		if (isNoInstances()) {
			LOGGER.warn("no instances left");
		}
		balanceChains();
	}

	@Override
	public boolean removeRules(List<InternalMatchRule> removedRules,
			Middlebox mb) {

		_foreman.deallocateRule(removedRules);
		return true;
	}

	@Override
	public boolean addRules(List<InternalMatchRule> rules, Middlebox mb) {
		if (_policyChains == null || _policyChains.isEmpty()) {
			LOGGER.error("cant handle rules if no policy chain exists");
			return false;
		}
		if (isNoInstances()) {
			LOGGER.error("cant handle rules there are no instances");
			return false;
		}
		ServiceInstance instnace = null;
		for (PolicyChain chain : _policyChains) {
			LOGGER.info("chain: " + chain.chain);
			if (chain.chain.contains(mb)) {
				instnace = _chainInstance.get(chain.trafficClass);
				LOGGER.info("Adding rules to instance: " + instnace);
				_foreman.assignRules(rules, instnace);
			}
		}
		if (instnace == null) {
			LOGGER.error("no instance is assigned to middlebox: " + mb);
			return false;
		}

		return true;
	}

	@Override
	public void setForeman(IDPIServiceFormen foreman) {
		_foreman = foreman;
	}

	@Override
	public void setPolicyChains(List<PolicyChain> chains) {
		if (chains.equals(_policyChains)) {
			LOGGER.info("no change in policy chains");
			return;
		}
		_policyChains = chains;
		LOGGER.info("policy chains updated: " + chains);
		balanceChains();
	}

	public ServiceInstance getChainInstance(PolicyChain chain) {
		return _chainInstance.get(chain.trafficClass);
	}

}
