package Common.Protocol;

/**
 * This class represents de-registration message from the Middlebox to the controller
 * this class serialized to/from json message
 * Created by Lior on 13/11/2014.
 */
public class MiddleboxDeregister extends MiddleboxRegister {
    public MiddleboxDeregister(String _middleboxId) {
        super(_middleboxId);
    }
}
