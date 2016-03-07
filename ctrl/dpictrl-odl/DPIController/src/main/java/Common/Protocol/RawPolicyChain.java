package Common.Protocol;

import java.net.InetAddress;
import java.util.List;

public class RawPolicyChain {

	public List<InetAddress> chain;
	public String trafficClass;

	/*
	 * (non-Javadoc)
	 * 
	 * @see java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		return "RawPolicyChain [trafficClass=" + trafficClass + ", chain="
				+ chain + "]";
	}

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
		RawPolicyChain other = (RawPolicyChain) obj;
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

}
