package org.opendaylight.dpi_tsa;

import java.util.List;

import Common.Protocol.RawPolicyChain;

public interface ITrafficSteeringService {

	public void applyPolicyChain(List<RawPolicyChain> chain);

	public List<RawPolicyChain> getPolicyChains();

}