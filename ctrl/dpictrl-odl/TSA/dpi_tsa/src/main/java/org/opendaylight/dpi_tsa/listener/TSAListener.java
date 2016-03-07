package org.opendaylight.dpi_tsa.listener;

import java.util.List;

import org.opendaylight.dpi_tsa.ITrafficSteeringService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import Common.JsonUtils;
import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.PolicyChainRequest;
import Common.Protocol.PolicyChainsData;
import Common.Protocol.RawPolicyChain;

public abstract class TSAListener {

	private ITrafficSteeringService _tsa;
	static final Logger logger = LoggerFactory.getLogger(TSAListener.class);

	public TSAListener() {
	}

	protected void handleIncomingMessage(String msg) {
		DPIProtocolMessage msgObject = JsonUtils.fromJson(msg);
		if (msgObject instanceof PolicyChainsData) {
			logger.info("got PolicyChainsData from controller: ");
			_tsa.applyPolicyChain(((PolicyChainsData) msgObject).chains);
		} else if (msgObject instanceof PolicyChainRequest) {
			logger.info("got PolicyChainRequest from controller");
			sendPolicyChains();
		} else {
			logger.warn("unknown message type");
		}
	}

	public void sendPolicyChains() {
		List<RawPolicyChain> chains = _tsa.getPolicyChains();
		try {
			sendMessage(JsonUtils.toJson(new PolicyChainsData(chains)));
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	protected abstract void sendMessage(String json);

	/**
	 * Function called by the dependency manager before the services exported by
	 * the component are unregistered, this will be followed by a "destroy ()"
	 * calls
	 * 
	 */
	public abstract void start();

	/**
	 * Function called by the dependency manager before the services exported by
	 * the component are unregistered, this will be followed by a "destroy ()"
	 * calls
	 * 
	 */
	public abstract void stop();

	/**
	 * Function called by the dependency manager when all the required
	 * dependencies are satisfied
	 * 
	 */
	public void init() {
		logger.info("Initialized");
	}

	/**
	 * Function called by the dependency manager when at least one dependency
	 * become unsatisfied or when the component is shutting down because for
	 * example bundle is being stopped.
	 * 
	 */
	void destroy() {
		this.stop();
	}

	public void unsetTSA(ITrafficSteeringService tsa) {
		if (_tsa == tsa) {
			logger.debug("TrafficSteeringService unset");
			_tsa = null;
		}
	}

	public void setTSA(ITrafficSteeringService tsa) {
		logger.debug("TrafficSteeringService set");
		this._tsa = tsa;
	}

}