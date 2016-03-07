package Common.Protocol;

import java.util.LinkedList;
import java.util.List;

/**
 * Created by Lior on 13/11/2014.
 */
public class MiddleboxRulesetAdd extends MiddleboxMessage {
    public List<MatchRule> rules;

    public MiddleboxRulesetAdd(String middleboxId) {
        super(middleboxId);
        rules = new LinkedList<MatchRule>();
    }
}
