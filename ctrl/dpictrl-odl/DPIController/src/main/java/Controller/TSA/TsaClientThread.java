package Controller.TSA;

import java.util.List;

import org.apache.log4j.Logger;

import Common.JsonUtils;
import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.PolicyChainRequest;
import Common.Protocol.PolicyChainsData;
import Common.Protocol.RawPolicyChain;

import com.google.gson.Gson;

public abstract class TsaClientThread extends Thread {

	protected static final Logger LOGGER = Logger
			.getLogger(TSASocketClient.class);
	protected final TSAFacadeImpl _tsaFacade;

	public TsaClientThread(TSAFacadeImpl tsaFacade) {
		super();
		this._tsaFacade = tsaFacade;
	}

	abstract void waitForInstructions();

	abstract void connectToTSA() throws Exception;

	abstract boolean sendMessage(String policyChainRequest);

	public static String generatePolicyChainMessage(List<RawPolicyChain> chains) {
		return JsonUtils.toJson(new PolicyChainsData(chains));
	}

	public static PolicyChainsData parsePolicyChain(String msg) {
		DPIProtocolMessage chain = JsonUtils.fromJson(msg);
		if (chain instanceof PolicyChainsData) {
			return (PolicyChainsData) chain;
		} else {
			return null;
		}
	}

	@Override
	public void run() {
		try {
			connectToTSA();
			sleep(3000);
			sendPolicyChainRequest();
			waitForInstructions();
		} catch (Exception e) {
			LOGGER.error("cant connect to TSA",e);
		}
	}

	private void sendPolicyChainRequest() {
		LOGGER.info("sending TSA request");
		sendMessage(getPolicyChainRequest());
	}

	String getPolicyChainRequest() {
		return new Gson().toJson(new PolicyChainRequest());
	}

	public void sendPolicyChains(List<RawPolicyChain> chains) {
		LOGGER.info("updating policyChains: ");
		LOGGER.info(chains);
		sendMessage(generatePolicyChainMessage(chains));
	}

	protected void handleIncomingMessage(String inputLine) {
		List<RawPolicyChain> chain = parsePolicyChain(inputLine).chains;
		LOGGER.info("got policyChain from TSA: ");
		LOGGER.info(chain);
		if (chain != null && chain.size() > 0) {
			LOGGER.info("valid chains");
			_tsaFacade.setPolicyChains(chain);
		}
	}

}
