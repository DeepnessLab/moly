package Controller;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import java.util.Arrays;
import java.util.List;
import java.util.Properties;

import org.junit.Test;
import org.mockito.ArgumentCaptor;

import Common.IChainNode;
import Common.Middlebox;
import Common.ServiceInstance;
import Common.Protocol.MatchRule;
import Controller.DPIForeman.DPIForeman;
import Controller.DPIServer.IDPIServiceFacade;

public class DPIControllerTest {

	@Test
	public void addInstance_overlappingRules() {

		Properties props = new Properties();
		props.setProperty("DPIServer.listeningPort", "5555");

		ConfigManager.setProperties(props);
		DPIController tested = new DPIController();

		DPIForeman foreman = (DPIForeman) tested.getForeman();
		IDPIServiceFacade mock = mock(IDPIServiceFacade.class);

		foreman.setInstancesFacade(mock);

		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");

		ServiceInstance s = new ServiceInstance("1");
		List<MatchRule> rules = Arrays.asList(new MatchRule("aaa", 1),
				new MatchRule("bbb", 2), new MatchRule("ccc", 1),
				new MatchRule("aaa", 2));
		tested.registerMiddlebox(mb1);
		tested.registerMiddlebox(mb2);
		tested.addRules(mb1, rules.subList(0, 2));
		tested.addRules(mb2, rules.subList(2, 4));
		foreman.setPolicyChains(Arrays.asList(new PolicyChain(Arrays.asList(
				(IChainNode) mb1, (IChainNode) mb2), "abc")));
		tested.registerInstance(s);

		ArgumentCaptor<List> captor = ArgumentCaptor.forClass(List.class);
		verify(mock).assignRules(captor.capture(),
				(ServiceInstance) anyObject());
		assertEquals(3, captor.getValue().size());
	}
}
