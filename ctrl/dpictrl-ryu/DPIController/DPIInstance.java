package DPIController;

import java.util.HashSet;
import java.util.LinkedList;
import java.util.logging.Level;


public class DPIInstance {
	private String _name;
	private int _index;
	private String _ip;
	private HashSet<String> _handledMiddleboxes;

	// pair of <policyChain without dpi instance, pcid>
	private LinkedList<Pair<String, String>> _policiesChains;

	public DPIInstance(String name, int index, String ip) {
		_name = name;
		_index = index;
		_ip = ip;
		_handledMiddleboxes = new HashSet<String>();
		_policiesChains = new LinkedList<Pair<String, String>>();
		DPIController.printLogMsg(Level.FINE, "DPIInstance", "create new DPI instance: name: '" + name + "', index: '" + index + "', ip: " + ip);
	}

	// the policy chain without this dpi instance
	public void addPolicyChain(String policyChain, String pcid) {
		_policiesChains.add(new Pair<String, String>(policyChain, pcid));

		String[] splitted = policyChain.split(",");

		for (int i=0; i<splitted.length; i++) {
			_handledMiddleboxes.add(splitted[i]);
			DPIController.printLogMsg(Level.FINE, "DPIInstance:addPolicyChain", "handle this middlebox: " + splitted[i]);
		}
	}

	public LinkedList<Pair<String, String>> getAllPoliciesChains() {
		return _policiesChains;
	}


	public int getNumOfHandledMiddleboxes() {
		return _handledMiddleboxes.size();
	}


	public HashSet<String> getHandledMiddleboxes() {
		return _handledMiddleboxes;
	}

	public String getIp() {
		return _ip;
	}


	public void setIp(String ip) {
		this._ip = ip;
	}


	public String getName() {
		return _name;
	}

	public int getIndex() {
		return _index;
	}


	@Override
	public String toString() {
		return "DPIInstance [name=" + _name + ", index=" + _index + "]";
	}
}
