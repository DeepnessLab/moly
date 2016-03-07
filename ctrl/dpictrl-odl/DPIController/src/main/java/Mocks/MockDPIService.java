package Mocks;

import Common.JsonUtils;
import Common.Protocol.InstanceDeregister;
import Common.Protocol.InstanceMessage;
import Common.Protocol.InstanceMessageFactory;
import Common.Protocol.InstanceRegister;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;

/**
 * this class is a mock DPI service which only try to connect to the controller
 * given as input and log every message recevied by the controller Created by
 * Lior on 20/11/2014.
 */
public class MockDPIService {

	private final InetAddress _controllerIp;
	private final int _controllerPort;
	private final String _id;
	private final String _name;
	private final InstanceMessageFactory _messageFactory;
	private Socket _socket;
	private PrintWriter _sendOut;

	public MockDPIService(InetAddress controllerIp, int controllerPort,
			String id, String name) {
		this._controllerIp = controllerIp;
		this._controllerPort = controllerPort;
		this._id = id;
		this._name = name;
		_messageFactory = new InstanceMessageFactory(id, name);
	}

	public void run() {
		Runtime.getRuntime().addShutdownHook(new Thread() {
			@Override
			public void run() {
				InstanceDeregister msg = _messageFactory.createDeregistration();
				sendMessageToController(msg);
			}
		});
		try {
			_socket = new Socket(_controllerIp.getHostAddress(),
					_controllerPort);
			_sendOut = new PrintWriter(_socket.getOutputStream(), true);
			System.out.println(String.format("dpi service %s:%s is up!", _id,
					_name));
			InstanceRegister msg = _messageFactory.createRegistration();
			sendMessageToController(msg);
			new ListenerMockThread("icmp").start();
			waitForInstructions();
		} catch (IOException e) {
			e.printStackTrace();
		}

	}

	private void sendMessageToController(InstanceMessage msg) {
		String registerMsg = JsonUtils.toJson(msg);
		_sendOut.println(registerMsg);
	}

	/**
	 * waits and prints every message received by controller
	 * 
	 * @throws IOException
	 */
	private void waitForInstructions() throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				_socket.getInputStream()));
		String inputLine;
		while ((inputLine = in.readLine()) != null) {
			System.out.println(String.format("%s received message: \n %s", _id,
					inputLine));
		}
	}

}
