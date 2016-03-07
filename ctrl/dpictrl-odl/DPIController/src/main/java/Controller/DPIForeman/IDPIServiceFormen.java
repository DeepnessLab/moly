package Controller.DPIForeman;

import java.util.Collection;
import java.util.List;

import Common.Middlebox;
import Common.ServiceInstance;
import Controller.InternalMatchRule;
import Controller.PolicyChain;

public interface IDPIServiceFormen {

	public boolean addWorker(ServiceInstance worker);

	public ILoadBalanceStrategy getStrategy();

	public void setStrategy(ILoadBalanceStrategy strategy);

	public void removeWorker(ServiceInstance removedWorker);

	public void removeJobs(List<InternalMatchRule> removedRules, Middlebox mb);

	/**
	 * add match rules to one or more instances using its load balancing
	 * strategy
	 * 
	 * @param internalRules
	 *            list of matchrules to divide among workers
	 * @param mb
	 * @return true if new jobs were added
	 */
	public boolean addJobs(List<InternalMatchRule> internalRules, Middlebox mb);

	public List<ServiceInstance> getInstances(InternalMatchRule rule);

	public List<InternalMatchRule> getRules(ServiceInstance worker);

	@Override
	public String toString();

	public Collection<ServiceInstance> getAllInstnaces();

	public List<ServiceInstance> getNeededInstances(
			List<InternalMatchRule> matchRules);

	public void assignRules(List<InternalMatchRule> rules,
			ServiceInstance worker);

	public void deallocateRule(List<InternalMatchRule> rules);

	public List<InternalMatchRule> getAllRules();

	public void setPolicyChains(List<PolicyChain> chains);

}