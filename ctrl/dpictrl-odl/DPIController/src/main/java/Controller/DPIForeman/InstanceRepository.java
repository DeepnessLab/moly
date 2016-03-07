package Controller.DPIForeman;

import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import Common.ServiceInstance;
import Common.Protocol.MatchRule;
import Controller.InternalMatchRule;

/**
 * Created by Lior on 24/11/2014.
 */
public class InstanceRepository {
	@Override
	public String toString() {
		return "InstanceRepository [_instancesMap=" + _instancesMap + "]";
	}

	private final Map<ServiceInstance, List<InternalMatchRule>> _instancesMap;
	private final Map<InternalMatchRule, List<ServiceInstance>> _rulesMap;

	public InstanceRepository() {
		_instancesMap = new HashMap<ServiceInstance, List<InternalMatchRule>>();
		_rulesMap = new HashMap<InternalMatchRule, List<ServiceInstance>>();
	}

	public void addInstance(ServiceInstance worker) {
		_instancesMap.put(worker, new LinkedList<InternalMatchRule>());
	}

	public List<InternalMatchRule> removeInstance(
			ServiceInstance removedInstance) {
		List<InternalMatchRule> matchRules = _instancesMap.get(removedInstance);
		for (MatchRule matchRule : matchRules) {
			_rulesMap.remove(matchRule);
		}
		_instancesMap.remove(removedInstance);
		return matchRules;
	}

	public List<ServiceInstance> getInstances(InternalMatchRule rule) {
		return _rulesMap.get(rule);
	}

	public List<InternalMatchRule> getRules(ServiceInstance worker) {
		return _instancesMap.get(worker);
	}

	public void removeRule(MatchRule rule) {
		List<ServiceInstance> instances = _rulesMap.get(rule);
		if (instances == null) {
			return;
		}
		for (ServiceInstance instance : instances) {
			List<InternalMatchRule> rules = _instancesMap.get(instance);
			rules.remove(rule);
		}

		_rulesMap.remove(rule);
	}

	public void addRule(InternalMatchRule rule, ServiceInstance instnace) {
		_instancesMap.get(instnace).add(rule);
		if (_rulesMap.get(rule) == null)
			_rulesMap.put(rule, new LinkedList<ServiceInstance>());
		_rulesMap.get(rule).add(instnace);
	}

	public void addRules(List<InternalMatchRule> rules,
			ServiceInstance mostFreeWorker) {
		for (InternalMatchRule rule : rules) {
			addRule(rule, mostFreeWorker);
		}
	}

	public void removeRules(List<InternalMatchRule> rules) {
		for (MatchRule rule : rules) {
			removeRule(rule);
		}
	}

	public Collection<ServiceInstance> getInstances() {
		return _instancesMap.keySet();
	}

	public Set<InternalMatchRule> getAllRules() {
		return _rulesMap.keySet();

	}
}
