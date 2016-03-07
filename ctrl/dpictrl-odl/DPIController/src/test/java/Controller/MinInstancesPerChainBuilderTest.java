package Controller;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import Common.IChainNode;
import Common.Middlebox;
import Common.ServiceInstance;
import Controller.DPIForeman.MinChainsPerInstanceStrategy;
import Controller.TSA.IPolicyChainsBuilder;
import Controller.TSA.MinInstancesPerChainBuilder;

public class MinInstancesPerChainBuilderTest {

	@Test
	public void sanityTest() {
		MinChainsPerInstanceStrategy strategyMock = mock(MinChainsPerInstanceStrategy.class);
		ServiceInstance[] instances = getInstances(2);
		Middlebox[] middleboxes = getMiddleboxes(5);

		PolicyChain policyChain1 = new PolicyChain(Arrays.asList(
				(IChainNode) middleboxes[0], middleboxes[4], middleboxes[1]),
				"1");
		PolicyChain policyChain2 = new PolicyChain(Arrays.asList(
				(IChainNode) middleboxes[2], middleboxes[4], middleboxes[3]),
				"2");

		List<PolicyChain> chains = Arrays.asList(policyChain1, policyChain2);
		when(strategyMock.getChainInstance(policyChain1)).thenReturn(
				instances[0]);
		when(strategyMock.getChainInstance(policyChain2)).thenReturn(
				instances[1]);
		IPolicyChainsBuilder tested = new MinInstancesPerChainBuilder(
				strategyMock);
		List<PolicyChain> result = tested.addDPIInstancesToChains(chains);
		for (PolicyChain policyChain : result) {
			assertEquals(4, policyChain.chain.size());
			if (policyChain.trafficClass.equals(policyChain1.trafficClass)) {
				assertEquals(instances[0], policyChain.chain.get(0));

			} else {
				assertEquals(instances[1], policyChain.chain.get(0));
			}
		}

	}

	private static Middlebox[] getMiddleboxes(int count) {
		Middlebox[] result = new Middlebox[count];
		for (int i = 0; i < count; i++) {
			result[i] = new Middlebox(String.valueOf(i));
		}
		return result;
	}

	private ServiceInstance[] getInstances(int count) {
		ServiceInstance[] result = new ServiceInstance[count];
		for (int i = 0; i < count; i++) {
			result[i] = new ServiceInstance(String.valueOf(i));
		}
		return result;

	}

}
