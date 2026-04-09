#include "Logger.h"

int main() {
	try {
		Logger logger("log.txt");
		logger.log(LogLevel::INFO, "Starting application...");
		logger.log(LogLevel::INFO, "User {} logged in", "Alice");
		int user_id = 42;
		logger.log(LogLevel::INFO, "User ID: {}", user_id);
		std::string action = "login";
		double duration = 1.23;
		logger.log(LogLevel::INFO, "Action: {}, Duration: {} seconds", action, duration);
		logger.log(LogLevel::DEBUG, "This is a message whithout placeholder");
		logger.log(LogLevel::ERROR, "mutiple placehodlers : {}, {}, {}", "arg1", "arg2", "arg3", "extra_arg");
	} catch (const std::exception& ex) {
		std::cerr << "Error: " << ex.what() << std::endl;
	}
	

	
	return 0;
}
