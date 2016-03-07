package Controller;

import Common.Protocol.MatchRule;

public class InternalMatchRule extends MatchRule {

	public InternalMatchRule(MatchRule from, int newId) {
		super(from.pattern, newId);
		this.is_regex = from.is_regex;
		className = MatchRule.class.getSimpleName();
	}

	public InternalMatchRule(MatchRule from) {
		super(from.pattern, from.rid);
		this.is_regex = from.is_regex;
	}

	public InternalMatchRule(String pattern, int rid) {
		super(pattern, rid);
	}

}
