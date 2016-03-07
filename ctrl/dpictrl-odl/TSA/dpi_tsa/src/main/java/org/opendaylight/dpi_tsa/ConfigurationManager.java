package org.opendaylight.dpi_tsa;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetAddress;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import org.dom4j.Document;
import org.dom4j.Element;
import org.dom4j.io.SAXReader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import Common.Protocol.RawPolicyChain;

public class ConfigurationManager {
	private static ConfigurationManager INSTANCE;
	static final Logger logger = LoggerFactory
			.getLogger(ConfigurationManager.class);
	private static Properties prop = new Properties();
	private final String propFileName = "configuration/tsaConfig.properties";
	private final String chainsFileName;
	private ConfigChangedDelegation _delegation;

	private ConfigurationManager() throws FileNotFoundException, IOException {
		initialize();
		chainsFileName = this.getProperty("tsa_policy_filename");
		if (chainsFileName != null)
			new Thread(new ConfigurationChangeListener(chainsFileName)).start();
	}

	public static ConfigurationManager getInstance() {
		if (INSTANCE == null) {
			try {
				INSTANCE = new ConfigurationManager();
			} catch (IOException e) {
				logger.error("error in configuration file: " + e.getMessage());
			}
		}
		return INSTANCE;
	}

	public String getProperty(String key) {
		String property = prop.getProperty(key);
		return property;
	}

	public void setPolicyChangedDelegation(ConfigChangedDelegation delegation) {
		this._delegation = delegation;

	}

	public void initialize() throws FileNotFoundException, IOException {
		InputStream inputStream = new FileInputStream(new File(propFileName));
		prop.load(inputStream);
	}

	public List<RawPolicyChain> getPolicyChainsConfig() {
		if (chainsFileName == null)
			return null;
		File chainsFile = new File(chainsFileName);
		if (!chainsFile.exists())
			return null;
		List<RawPolicyChain> result = null;
		try {
			SAXReader reader = new SAXReader();
			Document document = reader.read(chainsFile);
			Element rootElement = document.getRootElement();
			result = new LinkedList<RawPolicyChain>();
			for (Iterator i = rootElement.elementIterator(); i.hasNext();) {
				RawPolicyChain tmp = new RawPolicyChain();
				Element element = (Element) i.next();
				String matchStr = element.element("TrafficClass")
						.getStringValue();
				tmp.trafficClass = matchStr;
				tmp.chain = new LinkedList<InetAddress>();
				Iterator hosts = element.element("Hosts").elementIterator();
				for (; hosts.hasNext();) {
					Element hostElement = (Element) hosts.next();
					InetAddress hostAddress = InetAddress.getByName(hostElement
							.getStringValue());
					tmp.chain.add(hostAddress);
				}
				result.add(tmp);
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
		return result;
	}

	public void configurationChanged() {
		if (_delegation != null)
			_delegation.onConfigurationChange(this);

	}
}
