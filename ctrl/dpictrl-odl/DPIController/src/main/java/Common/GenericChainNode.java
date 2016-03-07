package Common;

import java.net.InetAddress;

public class GenericChainNode implements IChainNode {
	@Override
	public String toString() {
		return "GenericChainNode [address=" + address + "]";
	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + ((address == null) ? 0 : address.hashCode());
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
		GenericChainNode other = (GenericChainNode) obj;
		if (address == null) {
			if (other.address != null)
				return false;
		} else if (!address.equals(other.address))
			return false;
		return true;
	}

	public InetAddress address;

	public GenericChainNode(InetAddress rawNode) {
		address = rawNode;
	}

	@Override
	public InetAddress GetAddress() {
		return address;
	}

}
