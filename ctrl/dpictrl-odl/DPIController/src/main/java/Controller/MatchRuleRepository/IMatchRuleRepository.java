package Controller.MatchRuleRepository;

import java.net.InetAddress;
import java.util.Collection;
import java.util.List;
import java.util.Set;

import Common.Middlebox;
import Common.Protocol.MatchRule;
import Controller.InternalMatchRule;

public interface IMatchRuleRepository {

	public abstract boolean addMiddlebox(Middlebox mb);

	/**
	 * @param middleboxId
	 * @return list of InternalMatchRules to be removed (no other middlebox has
	 *         them), null if no such middlebox
	 */
	public abstract List<InternalMatchRule> removeMiddlebox(Middlebox mb);

	/**
	 * remove MatchRules for a middlebox mb
	 * 
	 * @param mb
	 * @param rules
	 *            list of rule ids only (encapsulated)
	 * @return list of removed internal rules (no other middlebox had them),
	 *         null if not such mb
	 */
	public abstract List<InternalMatchRule> removeRules(Middlebox mb,
			List<MatchRule> rules);

	/**
	 * this methods add rules to the repository corresponding to input middlebox
	 * existing rule-ids are overwritten the repository
	 * 
	 * @param middleboxId
	 *            the middlebox id that we want to add rules to
	 * @param rules
	 *            list of MatchRules we want to add
	 * @return list of internal rules with internal ids duplicate rules are
	 *         merged
	 */
	public abstract List<InternalMatchRule> addRules(Middlebox mb,
			List<MatchRule> rules);

	public abstract Collection<Middlebox> getAllMiddleboxes();

	public abstract Set<MatchRulePattern> getPatterns();

	public abstract List<MatchRule> getMatchRules(Middlebox mb,
			List<Integer> ruleIds);

	public abstract Middlebox getMiddelboxByAddress(InetAddress host);

	public abstract List<InternalMatchRule> getMatchRules(Middlebox mb);

}