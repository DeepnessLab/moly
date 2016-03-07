package wrappers;

import java.net.InetAddress;
import java.net.UnknownHostException;

import com.beust.jcommander.Parameter;

public class DPIServiceWrapperArgs {
	@Parameter(names = { "-controller_ip", "-ip" }, description = "IP address of the dpi controller", required = false)
	public String controller = "10.0.0.1";
	@Parameter(names = { "-controller_port", "-port" }, description = "port of the dpi controller", required = false)
	public short controllerPort = 5555;
	@Parameter(names = { "-instance_id", "-id" }, description = "id of the dpi instance", required = false)
	public String id;
	@Parameter(names = { "-instance_name", "-name" }, description = "name for the dpi instance", required = false)
	private String name;
	@Parameter(names = "-in", description = "input interface for the dpi instance (default is [id]-eth0)", required = false)
	private String inInterface;
	@Parameter(names = "-out", description = "output interface for dpi instance (default is [id]-eth0)", required = false)
	private String outInterface;
	@Parameter(names = "--print-packets", description = "print packets arriving to dpi instance (default bpf is udp)", required = false)
	public boolean printPackets = false;
	@Parameter(names = "--bpf", description = "bpf of packet to print (used with --print_packets)", required = false)
	public String bpf = "udp";

	public DPIServiceWrapperArgs() throws UnknownHostException {
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