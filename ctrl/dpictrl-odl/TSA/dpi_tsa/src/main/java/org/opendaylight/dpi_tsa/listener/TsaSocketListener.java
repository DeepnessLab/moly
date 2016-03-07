package org.opendaylight.dpi_tsa.listener;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class TsaSocketListener extends TSAListener {
	private TsaSocketListenerThread _thread;
	static final Logger logger = LoggerFactory
			.getLogger(TsaSocketListener.class);

	@Override
	protected void sendMessage(String msg) {
		_thread.sendMessage(msg);
	}

	@Override
	public void start() {
		logger.info("started");
		_thread.start();
	}

	@Override
	public void stop() {
		_thread.close();
		logger.info("stopped");
	}

	@Override
	public void init() {
		_thread = new TsaSocketListenerThread(this);
		logger.info("Initialized");
	}

}
