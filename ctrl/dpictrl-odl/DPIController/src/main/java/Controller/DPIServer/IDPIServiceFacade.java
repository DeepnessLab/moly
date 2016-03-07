package Controller.DPIServer;

import java.util.List;

import Common.ServiceInstance;
import Common.Protocol.ControllerMessage;
import Controller.InternalMatchRule;

/**
 * Created by Lior on 26/11/2014.
 */
public interface IDPIServiceFacade {

	public void deallocateRule(List<InternalMatchRule> instanceRules,
			ServiceInstance instance);

	public void assignRules(List<InternalMatchRule> rules,
			ServiceInstance instance);

	public boolean sendMessage(ServiceInstance instance, ControllerMessage msg);
}
