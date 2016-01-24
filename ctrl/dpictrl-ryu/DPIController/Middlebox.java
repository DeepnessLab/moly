package DPIController;

import java.util.HashMap;


public class Middlebox {
	private String _name;
	private int _index;
	private HashMap<String, HashMap<String, String>> _matchRules;
	private boolean _isFlow;
	private boolean _isStealth;

	public Middlebox(String name, int index, HashMap<String, HashMap<String, String>> matchRules, boolean isFlow, boolean isStealth) {
		_name = name;
		_index = index;
		_matchRules = matchRules;
		_isFlow = isFlow;
		_isStealth = isStealth;
	}


	public int getIndex() {
		return _index;
	}

	public String getName() {
		return _name;
	}

	public HashMap<String, HashMap<String, String>> getMatchRules() {
		return _matchRules;
	}

	public synchronized void setMatchRules (HashMap<String, HashMap<String, String>> newMatchRules) {
		_matchRules = newMatchRules;
	}

	public void printFullStatus() {
		System.out.println("Middlebox [name=" + _name + ", index=" + _index + ", matchRules:\n" + _matchRules + "]\n");
	}


	@Override
	public String toString() {
		return "Middlebox [name=" + _name + ", index=" + _index + ", # of matchRules="
				+ _matchRules.size() + "]";
	}
}
