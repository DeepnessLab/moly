package Controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.junit.Test;

import Common.Middlebox;
import Common.Protocol.MatchRule;
import Controller.MatchRuleRepository.IMatchRuleRepository;
import Controller.MatchRuleRepository.MatchRulePattern;
import Controller.MatchRuleRepository.MatchRulesRepository;

public class MatchRulesRepositoryTest {

	@Test
	public void testAddRules_aggregateRules() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		MatchRule[] rules1 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("bbb", 2) };
		MatchRule[] rules2 = new MatchRule[] { new MatchRule("ccc", 3),
				new MatchRule("bbb", 4) };

		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		List<InternalMatchRule> result1 = tested.addRules(mb1,
				Arrays.asList(rules1));
		List<InternalMatchRule> result2 = tested.addRules(mb2,
				Arrays.asList(rules2));
		Set<InternalMatchRule> set1 = new HashSet<InternalMatchRule>(result1);
		Set<InternalMatchRule> set2 = new HashSet<InternalMatchRule>(result2);
		set1.retainAll(set2);
		assertEquals(set1.size(), 1);
		MatchRule commonRule = (MatchRule) set1.toArray()[0];
		assertEquals(commonRule.pattern, "bbb");
	}

	@Test
	public void testRemoveMiddlebox_noRules() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		List<InternalMatchRule> rules = tested.removeMiddlebox(mb1);
		assertNull(rules);
		tested.addMiddlebox(mb1);
		rules = tested.removeMiddlebox(mb1);
		assertEquals(rules.size(), 0);
		tested.addMiddlebox(mb1);
		rules = tested.removeMiddlebox(mb2);
		assertNull(rules);
	}

	@Test
	public void testRemoveMiddlebox_withRules() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		MatchRule[] rules1 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("bbb", 2) };
		MatchRule[] rules2 = new MatchRule[] { new MatchRule("ccc", 3),
				new MatchRule("bbb", 4) };
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		tested.addRules(mb1, Arrays.asList(rules1));
		tested.addRules(mb2, Arrays.asList(rules2));
		assertEquals(tested.getPatterns().size(), 3);
		List<InternalMatchRule> rulesToRemove1 = tested.removeMiddlebox(mb1);
		assertEquals(tested.getPatterns().size(), 2);
		List<InternalMatchRule> rulesToRemove2 = tested.removeMiddlebox(mb2);
		assertEquals(rulesToRemove1.size(), 1);
		assertEquals(rulesToRemove2.size(), 2);
		assertEquals(tested.getPatterns().size(), 0);

	}

	@Test
	public void testRemoveRules_aggregateRules() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		MatchRule[] rules1 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("bbb", 2) };
		MatchRule[] rules2 = new MatchRule[] { new MatchRule("ccc", 3),
				new MatchRule("bbb", 4) };

		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		tested.addRules(mb1, Arrays.asList(rules1));
		tested.addRules(mb2, Arrays.asList(rules2));

		tested.removeMiddlebox(mb1);
		Set<MatchRulePattern> patterns = tested.getPatterns();
		assertTrue(patterns.contains(new MatchRulePattern(rules1[1])));
		tested.removeMiddlebox(mb2);
		patterns = tested.getPatterns();
		assertFalse(patterns.contains(new MatchRulePattern(rules1[1])));

	}

	@Test
	public void testAddRules_duplicatePatternDifferentMiddlebox() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		MatchRule matchRule2 = new MatchRule("aaa", 2);
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<InternalMatchRule> internalMatch2 = tested.addRules(mb2,
				Arrays.asList(matchRule2));
		assertEquals(internalMatch1.get(0).rid, internalMatch2.get(0).rid);
	}

	@Test
	public void testAddRules_duplicateRuleDifferentMiddlebox() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		MatchRule matchRule2 = new MatchRule("aaa", 1);
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<InternalMatchRule> internalMatch2 = tested.addRules(mb2,
				Arrays.asList(matchRule2));
		assertEquals(internalMatch1.get(0).rid, internalMatch2.get(0).rid);
	}

	@Test
	public void testAddRules_duplicatePatternSameMiddlebox() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		MatchRule matchRule2 = new MatchRule("aaa", 2);
		tested.addMiddlebox(mb1);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<InternalMatchRule> internalMatch2 = tested.addRules(mb1,
				Arrays.asList(matchRule2));
		assertEquals(internalMatch1.get(0).rid, internalMatch2.get(0).rid);
	}

	@Test
	public void testcase_1() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		MatchRule[] rules1 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("bbb", 2) };
		MatchRule[] rules2 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("ccc", 3) };
		MatchRule[] rules3 = new MatchRule[] { new MatchRule("aaa", 1),
				new MatchRule("ddd", 4) };
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		tested.addRules(mb1, Arrays.asList(rules1));
		tested.addRules(mb1, Arrays.asList(rules2));
		tested.addRules(mb2, Arrays.asList(rules3));
		List<InternalMatchRule> removeRules = tested.removeRules(mb2,
				Arrays.asList(new MatchRule("aaa", 1)));
		assertEquals(0, removeRules.size());
	}

	@Test
	public void testAddRules_duplicateRuleSameMiddlebox() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		MatchRule matchRule2 = new MatchRule("aaa", 1);
		tested.addMiddlebox(mb1);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<InternalMatchRule> internalMatch2 = tested.addRules(mb1,
				Arrays.asList(matchRule2));
		assertEquals(internalMatch1.get(0).rid, internalMatch2.get(0).rid);
		List<InternalMatchRule> removeRules = tested.removeRules(mb1,
				Arrays.asList(matchRule1));
		assertEquals(1, removeRules.size());
	}

	@Test
	public void testRemoveRules_sameRuleId() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		Middlebox mb2 = new Middlebox("2");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		MatchRule matchRule2 = new MatchRule("bbb", 1);
		tested.addMiddlebox(mb1);
		tested.addMiddlebox(mb2);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<InternalMatchRule> internalMatch2 = tested.addRules(mb2,
				Arrays.asList(matchRule2));
		assertNotEquals(internalMatch1.get(0).rid, internalMatch2.get(0).rid);
		internalMatch1 = tested.removeRules(mb1, Arrays.asList(matchRule1));
		internalMatch2 = tested.removeRules(mb2, Arrays.asList(matchRule2));
		assertEquals(internalMatch1.size(), 1);
		assertEquals(internalMatch2.size(), 1);

	}

	@Test
	public void testRemoveRules_sanity() {
		IMatchRuleRepository tested = new MatchRulesRepository();
		Middlebox mb1 = new Middlebox("1");
		MatchRule matchRule1 = new MatchRule("aaa", 1);
		tested.addMiddlebox(mb1);
		List<InternalMatchRule> internalMatch1 = tested.addRules(mb1,
				Arrays.asList(matchRule1));
		List<MatchRule> matchRules = tested.getMatchRules(mb1,
				Arrays.asList(matchRule1.rid));
		internalMatch1 = tested.removeRules(mb1, matchRules);
		assertEquals(1, internalMatch1.size());
	}
}
