package wrappers;

import java.net.InetAddress;
import java.net.UnknownHostException;

import com.beust.jcommander.Parameter;

public class IDSWrapperArgs {
	@Parameter(names = { "-controller_ip", "-ip" }, description = "IP address of the dpi controller", required = false)
	public String controller = "10.0.0.1";
	@Parameter(names = { "-controller_port", "-port" }, description = "port of the dpi controller", required = false)
	public short controllerPort = 5555;
	@Parameter(names = { "-middlebox_id", "-id" }, description = "id of the middlebox", required = true)
	public String id;
	@Parameter(names = { "-middlebox_name", "-name" }, description = "name for the middlebox", required = false)
	private String name;
	@Parameter(names = "-in", description = "input interface for the middlebox (default is [id]-eth0)", required = false)
	private String inInterface;
	@Parameter(names = "-out", description = "output interface for the middlebox (default is [id]-eth0)", required = false)
	private String outInterface;
	@Parameter(names = "--print-packets", description = "print packets arriving to the middlebox (default bpf is udp)", required = false)
	public boolean printPackets = false;
	@Parameter(names = "--bpf", description = "bpf of packet to print (used with --print_packets)", required = false)
	public String bpf = "udp";
	@Parameter(names = "--rules", description = "path to a MatchRules file", required = false)
	public String rulesFile;
	@Parameter(names = "--max-rules", description = "number of rules to load from rules_file", required = false)
	public int maxRules = -1;
	@Parameter(names = "--interactive", description = "use if you want interactive console", required = false)
	public boolean interactive = false;

	public IDSWrapperArgs() throws UnknownHostException {
		id = InetAddress.getLocalHost().getHostName();
	}

	public String getName() {
		return (name == null) ? id : name;
	}

	public String getInInterface() {
		return (inInterface == null) ? (id + "-eth0") : inInterface;
	}

	public String getOutInterface() {
		return (outInterface == null) ? (id + "-eth0") : outInterface;
	}

}