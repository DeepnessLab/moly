package Controller;

import java.io.InputStream;
import java.util.Properties;

import org.apache.log4j.Logger;

public class ConfigManager {
	protected static final Logger LOGGER = Logger
			.getLogger(ConfigManager.class);

	private static Properties _props = null;

	private static void initProperties() {
		try {// TODO: move to singleton
			InputStream input = ConfigManager.class.getClassLoader()
					.getResourceAsStream("config.properties");
			// FileReader input = new FileReader(new File("config.properties"));
			_props = new Properties();
			_props.load(input);
		} catch (Exception e) {
			LOGGER.error("config.properties missing!");
		}
	}

	public static String getProperty(String key) {
		if (_props == null)
			initProperties();
		return _props.getProperty(key);
	}

	public static void setProperties(Properties props) {
		_props = props;
	}

}
