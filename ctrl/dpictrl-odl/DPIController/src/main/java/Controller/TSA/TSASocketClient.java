package Controller.TSA;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;

import org.apache.log4j.MDC;

import Controller.ConfigManager;

public class TSASocketClient extends TsaClientThread {
	private Socket _socket;
	private InetAddress _tsaIp;
	private short _tsaPort;
	private PrintWriter _out;

	public TSASocketClient(TSAFacadeImpl tsaFacadeImpl) {
		super(tsaFacadeImpl);
		MDC.put("type", "Controller");
	}

	@Override
	void connectToTSA() throws Exception {
		_socket = new Socket();
		String ipAddress = ConfigManager
				.getProperty("TSAClient.interfaceAddress");
		_tsaIp = Inet4Address.getByName(ConfigManager
				.getProperty("TSAClient.TSAaddress"));
		_tsaPort = Short
				.valueOf(ConfigManager.getProperty("TSAClient.TSAPort"));
		if (ipAddress != null)
			_socket.bind(new InetSocketAddress(Inet4Address
					.getByName(ipAddress), 0));
		InetSocketAddress inetSocketAddress = new InetSocketAddress(
				_tsaIp.getHostAddress(), _tsaPort);
		LOGGER.info(inetSocketAddress);
		_socket.connect(inetSocketAddress);
		LOGGER.info("Connected to tsa");
		_out = new PrintWriter(_socket.getOutputStream(), true);
	}

	@Override
	boolean sendMessage(String msg) {
		LOGGER.info("sending message: " + msg);
		_out.println(msg);
		return true;
	}

	@Override
	void waitForInstructions() {
		try {
			BufferedReader in = new BufferedReader(new InputStreamReader(
					_socket.getInputStream()));
			String inputLine;
			LOGGER.info("waiting for updates.. ");

			while ((inputLine = in.readLine()) != null) {
				handleIncomingMessage(inputLine);
			}
		} catch (IOException e) {
			LOGGER.error("TSAClient interupted: " + e.getMessage());
		}
	}
}
