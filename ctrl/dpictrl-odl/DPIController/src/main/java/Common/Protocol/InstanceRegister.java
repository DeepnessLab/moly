package Common.Protocol;

/**
 * Created by Lior on 20/11/2014.
 */
public class InstanceRegister extends InstanceMessage {
    public String name;

    public InstanceRegister(String serviceId) {
        super(serviceId);
    }
}
