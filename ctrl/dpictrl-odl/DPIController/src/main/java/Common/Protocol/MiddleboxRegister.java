package Common.Protocol;

/**
 * Created by Lior on 13/11/2014.
 */

public class MiddleboxRegister extends MiddleboxMessage {
    public String name;
    public String siblingId;
    public boolean flow;
    public boolean stealth;

    public MiddleboxRegister(String _middleboxId) {
        super(_middleboxId);
    }


}
