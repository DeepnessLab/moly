package Controller.DPIForeman;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import org.apache.log4j.Logger;

import Common.Middlebox;
import Common.ServiceInstance;
import Controller.InternalMatchRule;
import Controller.PolicyChain;
import Controller.DPIServer.IDPIServiceFacade;

/**
 * this class has a key job in the controller it got notified on all the match
 * rules and all the instances and divides the work (patterns) between the
 * instances(DPI-Services) it uses DPIInstancesStrategy to split the work
 * Created by Lior on 20/11/2014.
 */
public class DPIForeman implements IDPIServiceFormen {
	private static final Logger LOGGER = Logger.getLogger(DPIForeman.class);

	private final InstanceRepository _workers; // rules per instance

	private ILoadBalanceStrategy _strategy;

	private IDPIServiceFacade _instancesFacade;

	public DPIForeman(IDPIServiceFacade controller) {
		_instancesFacade = controller;
		_workers = new InstanceRepository();
	}

	public IDPIServiceFacade getInstancesFacade() {
		return _instancesFacade;
	}

	public void setInstancesFacade(IDPIServiceFacade instancesFacade) {
		this._instancesFacade = instancesFacade;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.DPIForeman.IDPIServiceFormen#addWorker(Common.ServiceInstance)
	 */
	@Override
	public boolean addWorker(ServiceInstance worker) {
		LOGGER.trace(String.format("Instance %s,%s is added", worker.id,
				worker.name));
		if (_workers.getInstances().contains(worker)) {
			return false;
		}
		_workers.addInstance(worker);
		_strategy.instanceAdded(worker);
		return true;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#getStrategy()
	 */
	@Override
	public ILoadBalanceStrategy getStrategy() {
		return _strategy;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.DPIForeman.IDPIServiceFormen#setStrategy(Controller.DPIForeman
	 * .ILoadBalanceStrategy)
	 */
	@Override
	public void setStrategy(ILoadBalanceStrategy strategy) {
		this._strategy = strategy;
		this._strategy.setForeman(this);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.DPIForeman.IDPIServiceFormen#removeWorker(Common.ServiceInstance
	 * )
	 */
	@Override
	public void removeWorker(ServiceInstance removedWorker) {
		LOGGER.trace(String.format("Instance %s is going to be removed",
				removedWorker.id));

		List<InternalMatchRule> rules = _workers.removeInstance(removedWorker);
		_strategy.instanceRemoved(removedWorker, rules);
		LOGGER.info(String.format("%s instance removed", removedWorker.id));
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#removeJobs(java.util.List,
	 * Common.Middlebox)
	 */
	@Override
	public void removeJobs(List<InternalMatchRule> removedRules, Middlebox mb) {
		_strategy.removeRules(removedRules, mb);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#addJobs(java.util.List,
	 * Common.Middlebox)
	 */
	@Override
	public boolean addJobs(List<InternalMatchRule> internalRules, Middlebox mb) {
		if (_workers.getInstances().size() == 0) {
			return false;
		}
		return _strategy.addRules(internalRules, mb);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#getInstance(Controller.
	 * InternalMatchRule)
	 */
	@Override
	public List<ServiceInstance> getInstances(InternalMatchRule rule) {
		return _workers.getInstances(rule);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.DPIForeman.IDPIServiceFormen#getRules(Common.ServiceInstance)
	 */
	@Override
	public List<InternalMatchRule> getRules(ServiceInstance worker) {
		return _workers.getRules(worker);
	}

	/**
	 * remove the rules from the instances, including "real" instances
	 * 
	 * @param rules
	 *            list of MatchRules to remove
	 */
	@Override
	public void deallocateRule(List<InternalMatchRule> rules) {
		HashMap<ServiceInstance, List<InternalMatchRule>> tmp = new HashMap<>();
		for (InternalMatchRule rule : rules) {
			List<ServiceInstance> instances = _workers.getInstances(rule);
			if (instances == null) {
				LOGGER.error("rule not allocated: " + rule);
				continue;
			}
			for (ServiceInstance instance : instances) {
				if (!tmp.containsKey(instance)) {
					tmp.put(instance, new LinkedList<InternalMatchRule>());
				}
				tmp.get(instance).add(rule);
			}
		}
		for (ServiceInstance instance : tmp.keySet()) {
			List<InternalMatchRule> instanceRules = tmp.get(instance);
			_instancesFacade.deallocateRule(instanceRules, instance);
		}
		_workers.removeRules(rules);
	}

	@Override
	public void assignRules(List<InternalMatchRule> rules,
			ServiceInstance worker) {
		List<InternalMatchRule> existingRules = _workers.getRules(worker);
		rules.removeAll(existingRules);
		if (!rules.isEmpty()) {
			_instancesFacade.assignRules(rules, worker);
			_workers.addRules(rules, worker);
		}
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#toString()
	 */
	@Override
	public String toString() {
		return _workers.toString();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.DPIForeman.IDPIServiceFormen#getAllInstnaces()
	 */
	@Override
	public Collection<ServiceInstance> getAllInstnaces() {
		return this._workers.getInstances();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.DPIForeman.IDPIServiceFormen#getNeededInstances(java.util.
	 * List)
	 */
	@Override
	public List<ServiceInstance> getNeededInstances(
			List<InternalMatchRule> matchRules) {
		Set<ServiceInstance> result = new HashSet<>();
		for (InternalMatchRule matchRule : matchRules) {
			List<ServiceInstance> instances = _workers.getInstances(matchRule);
			if (instances != null) {
				result.addAll(instances);
			}
		}
		return new LinkedList<ServiceInstance>(result);
	}

	@Override
	public List<InternalMatchRule> getAllRules() {
		return new LinkedList<>(_workers.getAllRules());
	}

	@Override
	public void setPolicyChains(List<PolicyChain> chains) {
		_strategy.setPolicyChains(chains);
	}

}
