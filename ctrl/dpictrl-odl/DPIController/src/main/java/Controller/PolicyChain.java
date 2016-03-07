package Controller;

import java.util.List;

import Common.IChainNode;

public class PolicyChain {

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + ((chain == null) ? 0 : chain.hashCode());
		result = prime * result
				+ ((trafficClass == null) ? 0 : trafficClass.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		PolicyChain other = (PolicyChain) obj;
		if (chain == null) {
			if (other.chain != null)
				return false;
		} else if (!chain.equals(other.chain))
			return false;
		if (trafficClass == null) {
			if (other.trafficClass != null)
				return false;
		} else if (!trafficClass.equals(other.trafficClass))
			return false;
		return true;
	}

	public String trafficClass;

	public PolicyChain(List<IChainNode> resultChain, String trafficClass) {
		chain = resultChain;
		this.trafficClass = trafficClass;
	}

	public List<IChainNode> chain;

	@Override
	public String toString() {
		return "PolicyChain [trafficClass=" + trafficClass + ", chain=" + chain
				+ "]";
	}
}
