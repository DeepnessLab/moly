package Common.Protocol;


/**
 * this class represent an abstract message from the dpi-Service to the dpi-controller
 * it should be inherit from by a specific message
 * this class serialized to/from json message
 * Created by Lior on 13/11/2014.
 */
public abstract class InstanceMessage extends DPIProtocolMessage {
    public String id;

    public InstanceMessage(String id) {
        this.id = id;
    }

}
