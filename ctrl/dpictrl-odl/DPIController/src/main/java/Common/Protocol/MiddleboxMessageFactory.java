package Common.Protocol;

import java.util.List;

/**
 * this class used to generate messages from the middlebox to the dpi-controller
 * Created by Lior on 13/11/2014.
 */
public class MiddleboxMessageFactory {
	private final String _id;
	private final String _name;

	/**
	 * @param id
	 *            the middlebox id we want to generate message from
	 * @param name
	 *            the middlebox name we want to generate message from
	 */
	public MiddleboxMessageFactory(String id, String name) {
		this._id = id;
		this._name = name;
	}

	public MiddleboxRegister createRegistration() {
		MiddleboxRegister registerMsg = new MiddleboxRegister(_id);
		registerMsg.name = _name;
		return registerMsg;
	}

	public MiddleboxDeregister createDeregistration() {
		return new MiddleboxDeregister(_id);
	}

	public MiddleboxRulesetAdd createRulesetAdd(List<MatchRule> rules) {
		MiddleboxRulesetAdd msg = new MiddleboxRulesetAdd(_id);
		msg.rules = rules;
		return msg;
	}

	public MiddleboxRulesetRemove createRulesetRemove(List<Integer> rulesIds) {
		MiddleboxRulesetRemove msg = new MiddleboxRulesetRemove(_id, _name);
		msg.rules = rulesIds;
		return msg;
	}

}
