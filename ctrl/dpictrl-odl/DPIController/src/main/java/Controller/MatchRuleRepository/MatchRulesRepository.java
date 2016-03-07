package Controller.MatchRuleRepository;

import java.net.InetAddress;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.log4j.Logger;

import Common.Middlebox;
import Common.Protocol.MatchRule;
import Controller.InternalMatchRule;

/**
 * This class is in charge on keeping track on all the match rules registered by
 * the middleboxes, this repository holds a map from the middleboxes rules-ids
 * to the internal ids in order to aggregate identical rules Created by Lior on
 * 17/11/2014.
 */
public class MatchRulesRepository implements IMatchRuleRepository {
	private static int _rulesCount = 0;
	private static final Logger LOGGER = Logger
			.getLogger(MatchRulesRepository.class);
	private final HashMap<Middlebox, Set<MatchRule>> _rulesDictionary;
	private final Map<MatchRulePattern, InternalRuleData> _globalRules;

	public MatchRulesRepository() {
		_rulesDictionary = new HashMap<Middlebox, Set<MatchRule>>();
		_globalRules = new HashMap<>();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#addMiddlebox(Common
	 * .Middlebox)
	 */
	@Override
	public boolean addMiddlebox(Middlebox mb) {
		if (_rulesDictionary.containsKey(mb)) {
			return false;
		}
		_rulesDictionary.put(mb, new HashSet<MatchRule>());
		return true;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#removeMiddlebox(Common
	 * .Middlebox)
	 */
	@Override
	public List<InternalMatchRule> removeMiddlebox(Middlebox mb) {
		Set<MatchRule> rules = _rulesDictionary.get(mb);
		if (rules == null) {
			return null;
		}
		List<InternalMatchRule> result = removeRules(mb,
				new LinkedList<MatchRule>(rules));
		_rulesDictionary.remove(mb);
		return result;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#removeRules(Common
	 * .Middlebox, java.util.List)
	 */
	@Override
	public List<InternalMatchRule> removeRules(Middlebox mb,
			List<MatchRule> rules) {
		Set<MatchRule> currentRules = _rulesDictionary.get(mb);
		if (currentRules == null) {
			LOGGER.warn("unknown middlebox id: " + mb.id);
			return null;
		}
		List<InternalMatchRule> rulesToRemove = new LinkedList<InternalMatchRule>();
		for (MatchRule rule : rules) {
			if (!currentRules.remove(rule)) {
				LOGGER.warn(String.format("No such rule %s in middlebox %s",
						rule.rid, mb.id));
			} else {
				InternalMatchRule tmp = removeFromRuleSet(mb, rule);
				if (tmp != null) {
					rulesToRemove.add(tmp);
				}
			}

		}
		return rulesToRemove;
	}

	private InternalMatchRule removeFromRuleSet(Middlebox mb, MatchRule rule) {
		InternalMatchRule tmp = null;
		MatchRulePattern pattern = new MatchRulePattern(rule);
		InternalRuleData matchRuleData = _globalRules.get(pattern);
		matchRuleData.middleboxes.remove(mb);
		if (matchRuleData.middleboxes.size() == 0) {
			_globalRules.remove(pattern);
			tmp = matchRuleData.rule;
		}
		return tmp;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#addRules(Common.Middlebox
	 * , java.util.List)
	 */
	@Override
	public List<InternalMatchRule> addRules(Middlebox mb, List<MatchRule> rules) {
		Set<MatchRule> currentRules = _rulesDictionary.get(mb);
		if (currentRules == null) {
			LOGGER.warn("unknown middlebox id: " + mb.id);
			return null;
		}
		currentRules.addAll(rules);
		Set<InternalMatchRule> result = new HashSet<InternalMatchRule>();
		for (MatchRule matchRule : currentRules) {
			InternalMatchRule tmp = addToRulesSet(mb, matchRule);
			result.add(tmp);
		}
		return new LinkedList<InternalMatchRule>(result);
	}

	private InternalMatchRule addToRulesSet(Middlebox mb, MatchRule matchRule) {
		MatchRulePattern rulePattern = new MatchRulePattern(matchRule);
		if (_globalRules.containsKey(rulePattern)) {
			InternalRuleData matchRuleData = _globalRules.get(rulePattern);
			matchRuleData.middleboxes.add(mb);
			return matchRuleData.rule;
		} else {
			Integer id = generateRuleId();
			InternalMatchRule newRule = new InternalMatchRule(matchRule, id);
			InternalRuleData ruleData = new InternalRuleData(newRule, mb);
			_globalRules.put(rulePattern, ruleData);
			return newRule;
		}
	}

	private int generateRuleId() {
		_rulesCount++;
		return _rulesCount;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#getAllMiddleboxes()
	 */
	@Override
	public Collection<Middlebox> getAllMiddleboxes() {
		return this._rulesDictionary.keySet();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see Controller.MatchRuleRepository.IMatchRuleRepository#getPatterns()
	 */
	@Override
	public Set<MatchRulePattern> getPatterns() {
		return _globalRules.keySet();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#getMatchRules(Common
	 * .Middlebox, java.util.List)
	 */
	@Override
	public List<MatchRule> getMatchRules(Middlebox mb, List<Integer> ruleIds) {
		Set<MatchRule> rules = _rulesDictionary.get(mb);
		List<MatchRule> result = new LinkedList<MatchRule>();
		if (rules == null)
			return null;
		for (MatchRule matchRule : rules) {
			if (ruleIds.contains(matchRule.rid)) {

				result.add(matchRule);
			}
		}
		return result;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#getMiddelboxByAddress
	 * (java.net.InetAddress)
	 */
	@Override
	public Middlebox getMiddelboxByAddress(InetAddress host) {
		Set<Middlebox> keySet = _rulesDictionary.keySet();
		for (Middlebox middlebox : keySet) {
			if (middlebox.address.equals(host)) {
				return middlebox;
			}
		}
		return null;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * Controller.MatchRuleRepository.IMatchRuleRepository#getMatchRules(Common
	 * .Middlebox)
	 */
	@Override
	public List<InternalMatchRule> getMatchRules(Middlebox mb) {
		Set<MatchRule> rules = _rulesDictionary.get(mb);
		if (rules == null)
			return null;
		List<InternalMatchRule> result = new LinkedList<InternalMatchRule>();
		for (MatchRule matchRule : rules) {
			result.add(_globalRules.get(new MatchRulePattern(matchRule)).rule);
		}
		return result;
	}

}
