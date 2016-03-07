package Controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.anyList;
import static org.mockito.Matchers.anyObject;

import static org.mockito.Mockito.*;

import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import org.junit.Test;

import Common.IChainNode;
import Common.Middlebox;
import Common.ServiceInstance;
import Common.Protocol.MatchRule;
import Controller.DPIForeman.DPIForeman;
import Controller.DPIForeman.IDPIServiceFormen;
import Controller.DPIForeman.ILoadBalanceStrategy;
import Controller.DPIForeman.MinChainsPerInstanceStrategy;
import Controller.DPIServer.IDPIServiceFacade;
import Controller.MatchRuleRepository.IMatchRuleRepository;
import Controller.MatchRuleRepository.MatchRulesRepository;

public class InstancePerChainStrategyTest {

	@Test
	public void sanityTest() {
		IDPIServiceFacade mock = getUselessFacadeMock();
		IDPIServiceFormen foreman = new DPIForeman(mock);
		MatchRulesRepository matchRules = new MatchRulesRepository();

		List<ServiceInstance> instances = Arrays.asList(
				new ServiceInstance("1"), new ServiceInstance("2"),
				new ServiceInstance("3"));
		List<PolicyChain> chains = new LinkedList<PolicyChain>();
		Middlebox middlebox1a = new Middlebox("1a");
		Middlebox middlebox1b = new Middlebox("1b");
		Middlebox middlebox2a = new Middlebox("2a");
		Middlebox middlebox2b = new Middlebox("2b");
		matchRules.addMiddlebox(middlebox1a);
		matchRules.addMiddlebox(middlebox1b);
		matchRules.addMiddlebox(middlebox2a);
		matchRules.addMiddlebox(middlebox2b);
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middlebox1a,
				(IChainNode) middlebox1b), "a"));
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middlebox2a,
				(IChainNode) middlebox2b), "b"));
		ILoadBalanceStrategy strategy = new MinChainsPerInstanceStrategy(
				matchRules);
		foreman.setStrategy(strategy);
		MatchRule rule1a = new MatchRule("a", 11);
		MatchRule rule1b = new MatchRule("a", 12);
		MatchRule rule2a = new MatchRule("b", 21);
		MatchRule rule2b = new MatchRule("b", 22);
		List<InternalMatchRule> internal1a = matchRules.addRules(middlebox1a,
				Arrays.asList(rule1a));
		foreman.addJobs(internal1a, middlebox1a);
		List<InternalMatchRule> internal2a = matchRules.addRules(middlebox2a,
				Arrays.asList(rule2a));
		foreman.addJobs(internal2a, middlebox2a);
		List<InternalMatchRule> internal1b = matchRules.addRules(middlebox1b,
				Arrays.asList(rule1b));
		foreman.addJobs(internal1b, middlebox1b);
		List<InternalMatchRule> internal2b = matchRules.addRules(middlebox2b,
				Arrays.asList(rule2b));
		foreman.addJobs(internal2b, middlebox2b);

		foreman.setPolicyChains(chains);
		foreman.addWorker(instances.get(0));
		foreman.addWorker(instances.get(1));
		foreman.addWorker(instances.get(2));

		assertEquals(foreman.getNeededInstances(internal1a).get(0), foreman
				.getNeededInstances(internal1b).get(0));
		assertEquals(foreman.getNeededInstances(internal2a).get(0), foreman
				.getNeededInstances(internal2b).get(0));
		assertNotEquals(foreman.getNeededInstances(internal1a).get(0), foreman
				.getNeededInstances(internal2a).get(0));
	}

	@Test
	public void moreChainsThanInstances() {
		IDPIServiceFacade mock = getUselessFacadeMock();
		IDPIServiceFormen foreman = new DPIForeman(mock);
		MatchRulesRepository matchRules = new MatchRulesRepository();

		List<ServiceInstance> instances = Arrays.asList(
				new ServiceInstance("1"), new ServiceInstance("2"),
				new ServiceInstance("3"));
		List<PolicyChain> chains = new LinkedList<PolicyChain>();
		Middlebox[] middleboxes = getMiddleboxes(6);
		for (Middlebox middlebox : middleboxes) {
			matchRules.addMiddlebox(middlebox);
		}

		chains.add(new PolicyChain(Arrays.asList((IChainNode) middleboxes[0],
				(IChainNode) middleboxes[1]), "a"));
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middleboxes[2],
				(IChainNode) middleboxes[3]), "b"));
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middleboxes[4],
				(IChainNode) middleboxes[5]), "c"));

		ILoadBalanceStrategy strategy = new MinChainsPerInstanceStrategy(
				matchRules);
		foreman.setStrategy(strategy);
		MatchRule[] rules = getMatchRules(6);
		InternalMatchRule[] internalRules = new InternalMatchRule[6];
		for (int i = 0; i < rules.length; i++) {
			internalRules[i] = matchRules.addRules(middleboxes[i],
					Arrays.asList(rules[i])).get(0);
			foreman.addJobs(Arrays.asList(internalRules[i]), middleboxes[i]);
		}

		foreman.setPolicyChains(chains);
		foreman.addWorker(instances.get(0));
		foreman.addWorker(instances.get(1));
		Set<ServiceInstance> neededInstances = new HashSet<ServiceInstance>();
		neededInstances.addAll(foreman.getNeededInstances(Arrays
				.asList(internalRules[0])));
		neededInstances.addAll(foreman.getNeededInstances(Arrays
				.asList(internalRules[2])));
		neededInstances.addAll(foreman.getNeededInstances(Arrays
				.asList(internalRules[5])));
		assertEquals(2, neededInstances.size());
		for (int i = 0; i < internalRules.length; i++) {
			assertEquals(1,
					foreman.getNeededInstances(Arrays.asList(internalRules[i]))
							.size());
		}

	}

	@Test
	public void overlappingChains() {
		IDPIServiceFacade mock = getUselessFacadeMock();
		IDPIServiceFormen foreman = new DPIForeman(mock);
		MatchRulesRepository matchRules = new MatchRulesRepository();
		Middlebox[] middleboxes = getMiddleboxes(5);
		MatchRule[] rules = getMatchRules(5);
		InternalMatchRule[] internalRules = new InternalMatchRule[5];
		for (int i = 0; i < middleboxes.length; i++) {
			Middlebox middlebox = middleboxes[i];
			matchRules.addMiddlebox(middlebox);
			internalRules[i] = matchRules.addRules(middlebox,
					Arrays.asList(rules[i])).get(0);
		}

		ServiceInstance[] instances = getInstances(2);
		List<PolicyChain> chains = new LinkedList<PolicyChain>();

		// mb:1 is joint to both chains
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middleboxes[0],
				(IChainNode) middleboxes[1], (IChainNode) middleboxes[2]), "a"));
		chains.add(new PolicyChain(Arrays.asList((IChainNode) middleboxes[3],
				(IChainNode) middleboxes[1], (IChainNode) middleboxes[4]), "b"));

		ILoadBalanceStrategy strategy = new MinChainsPerInstanceStrategy(
				matchRules);
		foreman.setStrategy(strategy);

		foreman.setPolicyChains(chains);
		foreman.addWorker(instances[0]);
		foreman.addWorker(instances[1]);

		assertEquals(1,
				foreman.getNeededInstances(Arrays.asList(internalRules[0]))
						.size());

		assertEquals(1,
				foreman.getNeededInstances(Arrays.asList(internalRules[2]))
						.size());
		assertEquals(1,
				foreman.getNeededInstances(Arrays.asList(internalRules[3]))
						.size());
		assertEquals(1,
				foreman.getNeededInstances(Arrays.asList(internalRules[4]))
						.size());
		assertEquals(2,
				foreman.getNeededInstances(Arrays.asList(internalRules[1]))
						.size());

	}

	@Test
	public void testScenario_sameRule2chains_rulesBeforeWorkers() {

		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		IDPIServiceFormen tested = new DPIForeman(mock);
		IMatchRuleRepository matchRules = new MatchRulesRepository();
		tested.setStrategy(new MinChainsPerInstanceStrategy(matchRules));
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		MatchRule mr1 = new MatchRule("aaa", 1);
		MatchRule mr2 = new MatchRule("aaa", 1);
		matchRules.addMiddlebox(mb1);
		matchRules.addMiddlebox(mb2);
		List<InternalMatchRule> rules1 = matchRules.addRules(mb1,
				Arrays.asList(mr1));
		List<InternalMatchRule> rules2 = matchRules.addRules(mb2,
				Arrays.asList(mr2));
		assertEquals(rules1, rules2);
		assertFalse(tested.addJobs(rules1, mb1));
		assertFalse(tested.addJobs(rules2, mb2));
		PolicyChain chain1 = new PolicyChain(Arrays.asList((IChainNode) mb1),
				"a");
		PolicyChain chain2 = new PolicyChain(Arrays.asList((IChainNode) mb2),
				"b");
		tested.setPolicyChains(Arrays.asList(chain1, chain2));
		ServiceInstance ins1 = new ServiceInstance("1");
		ServiceInstance ins2 = new ServiceInstance("2");
		tested.addWorker(ins1);
		verify(mock).assignRules(rules1, ins1);
		tested.addWorker(ins2);
		verify(mock).assignRules(rules1, ins2);

	}

	@Test
	public void testScenario_sameRule2chains_workersBeforeRules() {

		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		IDPIServiceFormen tested = new DPIForeman(mock);
		IMatchRuleRepository matchRules = new MatchRulesRepository();
		tested.setStrategy(new MinChainsPerInstanceStrategy(matchRules));
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		matchRules.addMiddlebox(mb1);
		matchRules.addMiddlebox(mb2);
		PolicyChain chain1 = new PolicyChain(Arrays.asList((IChainNode) mb1),
				"a");
		PolicyChain chain2 = new PolicyChain(Arrays.asList((IChainNode) mb2),
				"b");
		tested.setPolicyChains(Arrays.asList(chain1, chain2));
		ServiceInstance ins1 = new ServiceInstance("1");
		ServiceInstance ins2 = new ServiceInstance("2");
		tested.addWorker(ins1);
		tested.addWorker(ins2);
		MatchRule mr1 = new MatchRule("aaa", 1);
		MatchRule mr2 = new MatchRule("aaa", 1);

		List<InternalMatchRule> rules1 = matchRules.addRules(mb1,
				Arrays.asList(mr1));
		List<InternalMatchRule> rules2 = matchRules.addRules(mb2,
				Arrays.asList(mr2));
		assertEquals(rules1, rules2);

		assertTrue(tested.addJobs(rules1, mb1));
		assertTrue(tested.addJobs(rules2, mb2));

		verify(mock).assignRules(rules1, ins2);
		verify(mock).assignRules(rules1, ins1);

	}

	@Test
	public void testScenario_sameRule2chains_sameMiddlebox() {

		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		IDPIServiceFormen tested = new DPIForeman(mock);
		IMatchRuleRepository matchRules = new MatchRulesRepository();
		tested.setStrategy(new MinChainsPerInstanceStrategy(matchRules));
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		matchRules.addMiddlebox(mb1);
		matchRules.addMiddlebox(mb2);
		PolicyChain chain1 = new PolicyChain(Arrays.asList((IChainNode) mb2,
				(IChainNode) mb1), "a");
		PolicyChain chain2 = new PolicyChain(Arrays.asList((IChainNode) mb1,
				(IChainNode) mb2), "b");
		tested.setPolicyChains(Arrays.asList(chain1, chain2));
		ServiceInstance ins1 = new ServiceInstance("1");
		ServiceInstance ins2 = new ServiceInstance("2");
		tested.addWorker(ins1);
		tested.addWorker(ins2);
		MatchRule mr1 = new MatchRule("aaa", 1);

		List<InternalMatchRule> rules1 = matchRules.addRules(mb1,
				Arrays.asList(mr1));

		assertTrue(tested.addJobs(rules1, mb1));

		verify(mock).assignRules(rules1, ins2);
		verify(mock).assignRules(rules1, ins1);

	}

	@Test
	public void testScenario_addAndRemoveUnneededInstance() {

		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		IDPIServiceFormen tested = new DPIForeman(mock);
		IMatchRuleRepository matchRules = new MatchRulesRepository();
		tested.setStrategy(new MinChainsPerInstanceStrategy(matchRules));
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		matchRules.addMiddlebox(mb1);
		matchRules.addMiddlebox(mb2);
		PolicyChain chain1 = new PolicyChain(Arrays.asList((IChainNode) mb2,
				(IChainNode) mb1), "a");
		PolicyChain chain2 = new PolicyChain(Arrays.asList((IChainNode) mb1,
				(IChainNode) mb2), "b");
		tested.setPolicyChains(Arrays.asList(chain1, chain2));
		ServiceInstance ins1 = new ServiceInstance("1");
		ServiceInstance ins2 = new ServiceInstance("2");
		ServiceInstance ins3 = new ServiceInstance("3");
		tested.addWorker(ins1);
		tested.addWorker(ins2);

		MatchRule mr1 = new MatchRule("aaa", 1);

		List<InternalMatchRule> rules1 = matchRules.addRules(mb1,
				Arrays.asList(mr1));

		assertTrue(tested.addJobs(rules1, mb1));
		tested.addWorker(ins3);
		tested.removeWorker(ins3);
		verify(mock).assignRules(rules1, ins2);
		verify(mock).assignRules(rules1, ins1);
		verifyNoMoreInteractions(mock);
	}

	private ServiceInstance[] getInstances(int count) {
		ServiceInstance[] result = new ServiceInstance[count];
		for (int i = 0; i < count; i++) {
			result[i] = new ServiceInstance(String.valueOf(i));
		}
		return result;

	}

	private static Middlebox[] getMiddleboxes(int count) {
		Middlebox[] result = new Middlebox[count];
		for (int i = 0; i < count; i++) {
			result[i] = new Middlebox(String.valueOf(i));
		}
		return result;
	}

	private static MatchRule[] getMatchRules(int count) {
		MatchRule[] result = new MatchRule[count];
		for (int i = 0; i < count; i++) {
			result[i] = new MatchRule(String.valueOf(i), i);
		}
		return result;
	}

	private IDPIServiceFacade getUselessFacadeMock() {
		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		doNothing().when(mock).assignRules(anyList(),
				(ServiceInstance) anyObject());
		return mock;
	}
}
