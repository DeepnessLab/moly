package Controller;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Collections;
import java.util.List;

import org.junit.Assert;
import org.junit.Test;

import Common.Protocol.RawPolicyChain;
import Controller.TSA.TSASocketClient;

public class TSAClientThreadTest {

	@Test
	public void sanityTest() throws UnknownHostException {

		List<RawPolicyChain> fromPolicyChains = Collections.nCopies(4,
				new RawPolicyChain());
		for (int i = 0; i < fromPolicyChains.size(); i++) {
			RawPolicyChain rawPolicyChain = fromPolicyChains.get(i);
			rawPolicyChain.trafficClass = String.valueOf(i);
			rawPolicyChain.chain = Collections.nCopies(i, InetAddress
					.getByName(String.format("%s.%s.%s.%s", i, i, i, i)));
		}
		String msg = TSASocketClient
				.generatePolicyChainMessage(fromPolicyChains);
		List<RawPolicyChain> toPolicyChains = TSASocketClient
				.parsePolicyChain(msg).chains;
		Assert.assertArrayEquals(fromPolicyChains.toArray(),
				toPolicyChains.toArray());

	}

}
