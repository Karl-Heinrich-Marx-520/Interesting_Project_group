#include "Logger.h"

Logger& Logger::getInstance() {
	static Logger logger;
	return logger;
}

Logger::Logger() {
	logFile.open("log.txt", std::ios::app);
	if (!logFile.is_open()) {
		throw std::runtime_error("Failure to open log file");
	}
}

Logger::~Logger() {
	if (logFile.is_open()) {
		logFile.close();
	}
}

void Logger::log(const std::string& message) {
	std::lock_guard<std::mutex> lock(mutex);
	if (logFile.is_open()) {
		auto now = std::chrono::system_clock::now();
		auto now_time = std::chrono::system_clock::to_time_t(now);
		char buffer[100];
		std::tm timeinfo;
		#ifdef _WIN32
			localtime_s(&timeinfo, &now_time);
		#else
			timeinfo = *std::localtime(&now_time);
		#endif
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
		logFile << std::string(buffer) << ": " << message << std::endl;
	}
}
