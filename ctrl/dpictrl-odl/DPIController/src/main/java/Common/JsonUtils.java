package Common;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

import Common.Protocol.DPIProtocolMessage;
import Common.Protocol.MatchRule;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;

/**
 * Created by Lior on 20/11/2014.
 */
public class JsonUtils {
	/**
	 * this static method generate a middlebox message object represent the json
	 * message string
	 * 
	 * @param message
	 *            a json string, represent a middlebox message
	 * @return sub type of MiddleboxMessage, the message object
	 */
	public static DPIProtocolMessage fromJson(String message) {
		Gson gson = new Gson();

		JsonElement msgTree = new JsonParser().parse(message);
		JsonElement className = msgTree.getAsJsonObject().get("className");

		try {
			return (DPIProtocolMessage) gson
					.fromJson(
							message,
							Class.forName("Common.Protocol."
									+ className.getAsString()));
			// TODO: make this more robust (dictionary or search in packages)
		} catch (ClassNotFoundException e) {
			return null;
		}

	}

	/**
	 * @param msg
	 * @return json representation of the input msg
	 */
	public static String toJson(DPIProtocolMessage msg) {
		Gson gson = new GsonBuilder().create();
		return gson.toJson(msg);
	}

	public static List<MatchRule> parseRulesFile(String filePath, int maxRules)
			throws IOException {
		BufferedReader br = new BufferedReader(new FileReader(filePath));
		String line;
		List<MatchRule> result = new LinkedList<MatchRule>();
		int i = 0;
		while ((line = br.readLine()) != null
				&& (i < maxRules || maxRules == -1)) {
			line = line.replaceAll("\\\\", "\\\\\\\\");
			MatchRule match = (MatchRule) fromJson(line);
			result.add(match);
			i++;
		}
		br.close();
		return result;
	}
}
