package Controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Matchers.anyList;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;

import java.util.LinkedList;
import java.util.List;

import org.junit.Test;

import Common.ServiceInstance;
import Controller.DPIForeman.DPIForeman;
import Controller.DPIForeman.IDPIServiceFormen;
import Controller.DPIForeman.SimpleLoadBalanceStrategy;
import Controller.DPIServer.IDPIServiceFacade;

public class DPIForemanTest {

	@org.junit.Test
	public void testLoadBalancing() throws Exception {
		IDPIServiceFacade mock = getUselessFacadeMock();
		IDPIServiceFormen foreman = new DPIForeman(mock);
		foreman.setStrategy(new SimpleLoadBalanceStrategy());

		foreman.addWorker(new ServiceInstance("I1", "Instance1"));
		foreman.addWorker(new ServiceInstance("I2", "Instance2"));
		List<InternalMatchRule> rules = new LinkedList<>();
		rules.add(new InternalMatchRule("1", 1));
		rules.add(new InternalMatchRule("2", 2));
		rules.add(new InternalMatchRule("3", 3));
		rules.add(new InternalMatchRule("4", 4));

		foreman.addJobs(rules.subList(0, 2), null);
		foreman.addJobs(rules.subList(2, 4), null);
		ServiceInstance instance2 = new ServiceInstance("I2");
		ServiceInstance instance1 = new ServiceInstance("I1");
		assertEquals(2, foreman.getRules(instance1).size());
		assertEquals(2, foreman.getRules(instance2).size());
	}

	private IDPIServiceFacade getUselessFacadeMock() {
		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);
		doNothing().when(mock).assignRules(anyList(),
				(ServiceInstance) anyObject());
		return mock;
	}

	@Test
	public void testNoInstances() throws Exception {
		IDPIServiceFacade mock = getUselessFacadeMock();
		IDPIServiceFormen foreman = new DPIForeman(mock);
		foreman.setStrategy(new SimpleLoadBalanceStrategy());
		List<InternalMatchRule> rules = new LinkedList<>();
		rules.add(new InternalMatchRule("1", 1));
		rules.add(new InternalMatchRule("2", 2));
		assertFalse(foreman.addJobs(rules, null));

	}

}