package wrappers;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.util.LinkedList;

import org.apache.log4j.Logger;
import org.apache.log4j.MDC;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.ParameterException;

import Mocks.ListenerMockThread;
import Mocks.MockMiddleBox;

public class IDSWrapper {
	private static final Logger LOGGER = Logger.getLogger(IDSWrapper.class);

	public static void main(String[] args) throws FileNotFoundException,
			IOException {
		MDC.put("type", "Middlebox");
		IDSWrapperArgs params = new IDSWrapperArgs();
		JCommander argsParser = new JCommander(params);
		try {
			argsParser.parse(args);
		} catch (ParameterException e) {
			LOGGER.error(e.getMessage());
			argsParser.usage();
			return;
		}
		MDC.put("id", params.id);
		LOGGER.info("starting IDS..");
		ExecutableWrapper processHandler = startIDS(params);
		try {
			InetAddress controllerIp = Inet4Address
					.getByName(params.controller);
			short controllerPort = params.controllerPort;

			MockMiddleBox middleBoxWrapper = new MockMiddleBox(controllerIp,
					controllerPort, params.id, params.getName());
			if (!params.interactive)
				middleBoxWrapper.setInteractice(false);
			middleBoxWrapper.start();
			if (params.printPackets)
				ListenerMockThread.startPrintingIncomingPackets(params.bpf,
						params.getInInterface());

			if (params.rulesFile != null) {
				middleBoxWrapper.loadRulesFile(params.rulesFile,
						params.maxRules);
			}
			middleBoxWrapper.join();
			LOGGER.info(String.format("Middlebox %s has stopped", params.id));

		} catch (Exception e) {
			StringWriter sw = new StringWriter();
			e.printStackTrace(new PrintWriter(sw));
			LOGGER.error(sw.toString());
		}
		processHandler.stopProcess();

	}

	private static ExecutableWrapper startIDS(IDSWrapperArgs params)
			throws FileNotFoundException, IOException {
		ExecutableWrapper processHandler = new ExecutableWrapper("/moly_ids",
				"ids_middlebox.exe");
		LinkedList<String> idsArgs = new LinkedList<String>();
		idsArgs.add("in=" + params.getInInterface());
		idsArgs.add("out=" + params.getOutInterface());
		processHandler.runProcess(idsArgs);
		return processHandler;
	}
}
