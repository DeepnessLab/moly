package Common.Protocol;

/**
 * Created by Lior on 20/11/2014.
 */
public class DPIProtocolMessage {
	@SuppressWarnings("unused")
	protected String className = getClassName();

	public String getClassName() {
		return getClass().getSimpleName();
	}
}
