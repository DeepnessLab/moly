package Common.Protocol;

/**
 * Created by Lior on 20/11/2014.
 */
public class InstanceException extends InstanceMessage {
    public int code;
    public String name;
    public String message;
    public String stacktrace;

    public InstanceException(String serviceId) {
        super(serviceId);
    }
}
