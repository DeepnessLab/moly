package Mocks;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import org.apache.log4j.Logger;
import org.jnetpcap.Pcap;
import org.jnetpcap.PcapBpfProgram;
import org.jnetpcap.PcapIf;
import org.jnetpcap.packet.PcapPacket;
import org.jnetpcap.packet.PcapPacketHandler;

import Controller.DPIController;

/**
 * this class is a thread which print any packet seen, according to bpf filter
 * the thread also has the capability to loop the packet back to the network
 * 
 * @author ubuntu
 * 
 */
public class ListenerMockThread extends Thread {
	private static final Logger LOGGER = Logger.getLogger(DPIController.class);
	private String _bpf = "";
	private PcapIf _inInterface;
	private PcapIf _outInterface;
	private boolean _loopPacket = false;

	public ListenerMockThread(boolean loopPacket) {
		super();
		_inInterface = getDefaultInterface();
		if (loopPacket)
			_outInterface = _inInterface;
		_loopPacket = true;
	}

	public ListenerMockThread(String inInterface) throws IOException {
		super();
		_inInterface = findInterface(inInterface);
		if (_inInterface == null) {
			throw new IOException("no such interface: " + inInterface);
		}
	}

	public ListenerMockThread(String inInterface, String outInterface)
			throws IOException {
		super(inInterface);
		_loopPacket = true;
		_outInterface = findInterface(outInterface);
		if (_outInterface == null) {
			throw new IOException("no such interface: " + outInterface);
		}
	}

	private PcapIf findInterface(String intr) {
		List<PcapIf> alldevs = new ArrayList<PcapIf>(); // Will be filled with
		// NICs
		StringBuilder errbuf = new StringBuilder(); // For any error msgs

		int r = Pcap.findAllDevs(alldevs, errbuf);
		if (r == Pcap.ERROR || alldevs.isEmpty()) {
			LOGGER.error(String.format(
					"Can't read list of devices, error is %s",
					errbuf.toString()));
			return null;
		}
		for (PcapIf pcapIf : alldevs) {

			if (pcapIf.getName().equalsIgnoreCase(intr)) {
				LOGGER.debug(intr + " found!");
				return pcapIf;
			}
		}
		LOGGER.error(intr + " not found");
		return null;
	}

	private PcapIf getDefaultInterface() {
		StringBuilder errbuf = new StringBuilder();
		List<PcapIf> alldevs = new ArrayList<PcapIf>();
		int r = Pcap.findAllDevs(alldevs, errbuf);
		return alldevs.get(2);
	}

	private final class LoopHandler implements PcapPacketHandler<Pcap> {
		@Override
		public void nextPacket(PcapPacket packet, Pcap device) {
			LOGGER.trace(String.format("Received packet at %s - %s \n",
					new Date(packet.getCaptureHeader().timestampInMillis()),
					generatePacketString(packet)));
			if (_loopPacket)
				loopPacket(packet, device);
		}

		private void loopPacket(PcapPacket packet, Pcap device) {
			try {
				Thread.sleep(3000);
			} catch (InterruptedException e) {
				LOGGER.error(e.getMessage());
			}
			if (device.sendPacket(packet) != Pcap.OK) {
				LOGGER.error(device.getErr());
			}
		}

		private String generatePacketString(PcapPacket packet) {
			return packet.toString();
		}
	}

	@Override
	public void run() {
		StringBuilder errbuf = new StringBuilder();
		int snaplen = 64 * 1024; // Capture all packets, no trucation
		int flags = Pcap.MODE_PROMISCUOUS; // capture all packets
		int timeout = -1; // 10 seconds in millis
		LOGGER.info("listening on interface: " + _inInterface.getName()
				+ " with bpf " + _bpf);
		Pcap pcap = Pcap.openLive(_inInterface.getName(), snaplen, flags,
				timeout, errbuf);

		if (pcap == null) {
			LOGGER.error("Error while opening device for capture: "
					+ errbuf.toString());
			return;
		}

		setFilter(pcap, getBPF());

		PcapPacketHandler<Pcap> jpacketHandler = new LoopHandler();

		pcap.loop(Pcap.LOOP_INTERRUPTED, jpacketHandler, pcap);

		pcap.close();
	}

	private void setFilter(Pcap pcap, String expression) {
		PcapBpfProgram program = new PcapBpfProgram();

		int optimize = 0;
		int netmask = 0xFFFFFFFF;

		if (pcap.compile(program, expression, optimize, netmask) != Pcap.OK) {
			LOGGER.error(pcap.getErr());
			return;
		}

		if (pcap.setFilter(program) != Pcap.OK) {
			LOGGER.error(pcap.getErr());
			return;
		}
		pcap.setFilter(program);
	}

	public String getBPF() {
		return _bpf;
	}

	public void setBPF(String _bpf) {
		this._bpf = _bpf;
	}

	public static void startPrintingIncomingPackets(String bpf, String inter)
			throws IOException {
		ListenerMockThread listener = new ListenerMockThread(inter);
		listener.setBPF(bpf);
		listener.start();
	}

}
