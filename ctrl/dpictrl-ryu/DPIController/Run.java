package DPIController;

public class Run {
public static void main(String[] args) {
		boolean isDebugMode = false;

		if (args.length > 0 && String.valueOf(args[0]).toLowerCase().startsWith("debug"))
			isDebugMode = true;

		int dpiControllerPort = 9091;
		int dpiInstListeningPort = 9092;
		new DPIController(dpiControllerPort, dpiInstListeningPort, isDebugMode);
	}
}
