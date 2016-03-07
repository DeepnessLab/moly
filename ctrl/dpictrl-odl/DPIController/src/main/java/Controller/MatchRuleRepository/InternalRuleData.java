package Controller.MatchRuleRepository;

import java.util.HashSet;
import java.util.Set;

import Common.Middlebox;
import Controller.InternalMatchRule;

public class InternalRuleData {
	public InternalRuleData(InternalMatchRule rule, Middlebox mb) {
		this.rule = rule;
		middleboxes = new HashSet<Middlebox>();
		middleboxes.add(mb);
	}

	public InternalMatchRule rule;
	public Set<Middlebox> middleboxes;

}
