package Common;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.MatchRule;

public class JsonUtilsTest {

	@Test
	public void fromJsonTest_MathcRule() {
		String str = "{ className: 'MatchRule', rid: 17919, is_regex: false, pattern: 'j|00||ba||13||04||83||ea||20|3|ff|>|8a||a6||0e||05|>(|a3||1a||01|G;|fa|u|f6|' }";
		DPIProtocolMessage fromJson = JsonUtils.fromJson(str);
		assertEquals(17919, ((MatchRule) fromJson).rid.intValue());
	}

	@Test
	public void fromJsonTest_MathcRuleBackSlash() {
		String str = "{ className: 'MatchRule', rid: 17919, is_regex: false, pattern: 'j|00||ba||13||04||83||ea||20|3|ff|>|8a||a6||0e||05|>\\(|a3||1a||01|G;|fa|u|f6|\\' }";
		DPIProtocolMessage fromJson = JsonUtils.fromJson(str.replaceAll("\\\\",
				"\\\\\\\\"));
		assertEquals(
				"j|00||ba||13||04||83||ea||20|3|ff|>|8a||a6||0e||05|>\\(|a3||1a||01|G;|fa|u|f6|\\",
				((MatchRule) fromJson).pattern);
	}
}
