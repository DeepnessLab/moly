package Controller.DPIForeman;

import java.util.List;

import Common.Middlebox;
import Common.ServiceInstance;
import Controller.InternalMatchRule;
import Controller.PolicyChain;

/**
 * Created by Lior on 24/11/2014.
 */
public interface ILoadBalanceStrategy {

	void instanceAdded(ServiceInstance instance);

	void instanceRemoved(ServiceInstance instance,
			List<InternalMatchRule> removedRules);

	boolean removeRules(List<InternalMatchRule> removedRules, Middlebox mb);

	boolean addRules(List<InternalMatchRule> internalRules, Middlebox mb);

	void setForeman(IDPIServiceFormen foreman);

	void setPolicyChains(List<PolicyChain> chains);
}
