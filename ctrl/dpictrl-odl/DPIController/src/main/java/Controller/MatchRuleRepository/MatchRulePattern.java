package Controller.MatchRuleRepository;

import Common.Protocol.MatchRule;

public class MatchRulePattern {
	public String pattern;
	public boolean is_regex;

	public MatchRulePattern(MatchRule rule) {
		pattern = rule.pattern;
		is_regex = rule.is_regex;

	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + (is_regex ? 1231 : 1237);
		result = prime * result + ((pattern == null) ? 0 : pattern.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		MatchRulePattern other = (MatchRulePattern) obj;
		if (is_regex != other.is_regex)
			return false;
		if (pattern == null) {
			if (other.pattern != null)
				return false;
		} else if (!pattern.equals(other.pattern))
			return false;
		return true;
	}

}
