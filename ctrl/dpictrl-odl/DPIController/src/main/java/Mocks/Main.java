package Mocks;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.UUID;

import org.apache.log4j.Logger;

import Controller.DPIController;

/**
 * this Main class is running mock service and controllers requested by the user
 */
public class Main {
	static String USAGE = "USAGE: controller_Ip controller_port service|middlebox [name]";
	private static final Logger LOGGER = Logger.getLogger(Main.class);

	public static void main(String[] args) {
		if (args.length < 3) {
			LOGGER.error(USAGE);
			return;
		}
		String id = UUID.randomUUID().toString();
		String controllerIpStr = args[0];
		String controllerPortStr = args[1];
		String mockType = args[2];
		String mockName;
		if (args.length == 4) {
			mockName = args[3];
		} else {
			mockName = mockType + id.substring(1, 4);
		}
		try {
			InetAddress controllerIp = Inet4Address.getByName(controllerIpStr);
			int controllerPort = Integer.parseInt(controllerPortStr);

			switch (mockType) {
			case "middlebox":
				MockMiddleBox mmd = new MockMiddleBox(controllerIp,
						controllerPort, id, mockName);
				new ListenerMockThread(true).start();
				mmd.run();
				break;
			case "service":
				MockDPIService mds = new MockDPIService(controllerIp,
						controllerPort, id, mockName);
				mds.run();
				break;
			default:
				LOGGER.error("unsupported mock type should be service or middlebox");
			}
		} catch (UnknownHostException e) {
			LOGGER.error(controllerIpStr + " is an invalid Ip address");
			LOGGER.error(USAGE);
		} catch (NumberFormatException e) {
			LOGGER.error(controllerPortStr + " is an invalid port address");
			LOGGER.error(USAGE);
		}

	}
}
