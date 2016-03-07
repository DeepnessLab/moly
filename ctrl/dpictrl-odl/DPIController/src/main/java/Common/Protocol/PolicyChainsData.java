package Common.Protocol;

import java.util.List;

public class PolicyChainsData extends DPIProtocolMessage {
	public PolicyChainsData(List<RawPolicyChain> result) {
		chains = result;
	}

	public List<RawPolicyChain> chains;
}
