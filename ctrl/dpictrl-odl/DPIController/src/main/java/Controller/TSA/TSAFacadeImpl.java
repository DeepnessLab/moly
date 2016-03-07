package Controller.TSA;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.apache.log4j.Logger;

import Common.GenericChainNode;
import Common.IChainNode;
import Common.Protocol.RawPolicyChain;
import Controller.DPIController;
import Controller.PolicyChain;

public class TSAFacadeImpl implements ITSAFacade {

	private static final Logger LOGGER = Logger.getLogger(TSAFacadeImpl.class);

	private List<PolicyChain> _currentChains;
	private final DPIController _dpiController;
	private final TsaClientThread _tsaClient;

	private List<RawPolicyChain> _currentRawChains;

	public TSAFacadeImpl(DPIController dpiController) {
		this._dpiController = dpiController;
		_currentRawChains = new LinkedList<RawPolicyChain>();
		_tsaClient = new TSASocketClient(this);
		_tsaClient.start();
	}

	@Override
	public boolean updatePolicyChains(List<PolicyChain> chains) {
		List<RawPolicyChain> rawChains = generateRawChains(chains);
		_tsaClient.sendPolicyChains(rawChains);
		LOGGER.info("send tsa message " + rawChains);
		return true;
	}

	void setPolicyChains(List<RawPolicyChain> rawChains) {
		_currentRawChains = rawChains;
		this.refreshPolicyChains();
	}

	private PolicyChain generatePolicyChain(RawPolicyChain rawChain) {
		List<IChainNode> knownNodes = new LinkedList<IChainNode>();
		knownNodes.addAll(_dpiController.getAllMiddleBoxes());
		knownNodes.addAll(_dpiController.getAllInstances());
		Map<InetAddress, IChainNode> knownNodesMap = new HashMap<InetAddress, IChainNode>();
		for (IChainNode node : knownNodes) {
			knownNodesMap.put(node.GetAddress(), node);
		}
		List<IChainNode> resultChain = new LinkedList<IChainNode>();
		for (InetAddress rawNode : rawChain.chain) {
			resultChain.add(knownNodesMap.containsKey(rawNode) ? knownNodesMap
					.get(rawNode) : new GenericChainNode(rawNode));
		}
		return new PolicyChain(resultChain, rawChain.trafficClass);
	}

	private List<RawPolicyChain> generateRawChains(List<PolicyChain> chains) {
		List<RawPolicyChain> result = new LinkedList<RawPolicyChain>();
		for (PolicyChain policyChain : chains) {
			RawPolicyChain tmp = new RawPolicyChain();
			tmp.trafficClass = policyChain.trafficClass;
			tmp.chain = new LinkedList<InetAddress>();
			for (IChainNode node : policyChain.chain) {
				tmp.chain.add(node.GetAddress());
			}
			result.add(tmp);
		}
		return result;
	}

	@Override
	public List<PolicyChain> getPolicyChains() {
		return _currentChains;
	}

	@Override
	public void refreshPolicyChains() {
		LinkedList<PolicyChain> newChains = new LinkedList<PolicyChain>();
		for (RawPolicyChain rawChain : _currentRawChains) {
			newChains.add(generatePolicyChain(rawChain));
		}
		if (newChains.equals(_currentChains))
			return;
		_currentChains = newChains;
		_dpiController.updatePolicyChains(_currentChains);
	}

}
