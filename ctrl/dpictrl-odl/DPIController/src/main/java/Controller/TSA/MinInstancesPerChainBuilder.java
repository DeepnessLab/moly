package Controller.TSA;

import java.util.LinkedList;
import java.util.List;

import Common.IChainNode;
import Common.ServiceInstance;
import Controller.PolicyChain;
import Controller.DPIForeman.MinChainsPerInstanceStrategy;

public class MinInstancesPerChainBuilder implements IPolicyChainsBuilder {

	private final MinChainsPerInstanceStrategy _strategy;

	public MinInstancesPerChainBuilder(MinChainsPerInstanceStrategy strategy) {
		this._strategy = strategy;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.TSA.IPolicyChainsBuilder#addDPIInstancesToChains(java.util
	 * .List)
	 */
	@Override
	public List<PolicyChain> addDPIInstancesToChains(
			List<PolicyChain> currentChains) {

		List<PolicyChain> newChains = new LinkedList<PolicyChain>();

		for (PolicyChain currentChain : currentChains) {
			List<IChainNode> newChain = new LinkedList<IChainNode>();
			ServiceInstance chainInstance = _strategy
					.getChainInstance(currentChain);
			if (chainInstance != null) {
				newChain.add(chainInstance);
			}
			newChain.addAll(removeInstances(currentChain.chain));
			newChains.add(new PolicyChain(newChain, currentChain.trafficClass));
		}

		return newChains;
	}

	public List<IChainNode> removeInstances(List<IChainNode> chain) {
		List<IChainNode> result = new LinkedList<IChainNode>();
		for (IChainNode node : chain) {
			if (!(node instanceof ServiceInstance)) {
				result.add(node);
			}
		}
		return result;
	}

}
