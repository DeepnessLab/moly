package Controller.TSA;

import java.util.List;

import Controller.PolicyChain;

public interface ITSAFacade {

	/**
	 * notify the TSA on the new policy chain
	 * 
	 * @param ordered
	 *            list of the addresses of the chain instances
	 * @return
	 */
	public boolean updatePolicyChains(List<PolicyChain> chains);

	public List<PolicyChain> getPolicyChains();

	public void refreshPolicyChains();

}
