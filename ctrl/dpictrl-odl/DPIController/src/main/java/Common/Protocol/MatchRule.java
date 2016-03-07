package Common.Protocol;

import java.util.ArrayList;
import java.util.List;

/**
 * This class represent a match rule in the MiddleBox, means a pattern (could be
 * regex or string) on which the Middlebox need to act uppon this class
 * serialized to/from json message Created by Lior on 13/11/2014.
 */
public class MatchRule extends DPIProtocolMessage {
	@Override
	public String toString() {
		return String
				.format("{ className: 'MatchRule', rid: %d, is_regex: %s, pattern: '%s' }",
						rid, is_regex, pattern);
	}

	public String pattern; // some exact-string or regular-expression string,
	public boolean is_regex; // true if pattern is a regular-expression or false
								// otherwise,
	public Integer rid; // rule identification number,

	public MatchRule(int ruleId) {
		rid = ruleId;
	}

	public MatchRule(String pattern, int rid) {
		this.pattern = pattern;
		this.rid = rid;
	}

	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (o == null || getClass() != o.getClass())
			return false;

		MatchRule matchRule = (MatchRule) o;

		if (!(rid.equals(matchRule.rid)))
			return false;

		return true;
	}

	@Override
	public int hashCode() {
		int result = rid.hashCode();
		return result;
	}

	public static List<MatchRule> create(List<Integer> rules) {
		List<MatchRule> result = new ArrayList<MatchRule>();
		for (Integer rule : rules) {
			result.add(new MatchRule(rule));
		}
		return result;
	}
	/*
	 * Future features: start-idx: start index in L7 payload, end-idx: start
	 * index in L7 payload, match-sets: array of match sets, policy-chains:
	 * array of policy chain IDs
	 */
}
