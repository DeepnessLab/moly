package Controller.TSA;

import java.util.List;

import Common.IChainNode;
import Controller.PolicyChain;

public interface IPolicyChainsBuilder {

	public abstract List<PolicyChain> addDPIInstancesToChains(
			List<PolicyChain> currentChains);

	List<IChainNode> removeInstances(List<IChainNode> chain);

}