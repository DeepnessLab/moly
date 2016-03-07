package Controller;

import java.io.IOException;
import java.net.URI;

import org.apache.log4j.Level;
import org.apache.log4j.Logger;
import org.glassfish.grizzly.http.server.HttpServer;
import org.glassfish.jersey.grizzly2.httpserver.GrizzlyHttpServerFactory;
import org.glassfish.jersey.server.ResourceConfig;

/**
 * main class used to run controller Created by Lior on 12/11/2014.
 */
public class Main {
	private static final URI BASE_URI = URI
			.create("http://localhost:8080/dpi/");

	public static void main(String[] args) {

		DPIController controller = new DPIController();
		controller.run();
	}

}
