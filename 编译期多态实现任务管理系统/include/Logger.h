#pragma once
#include <string>
#include <iostream>
#include <mutex>
#include <fstream>
#include <chrono>
#include <ctime>

class Logger {
public:
	//获取单例
	static Logger& getInstance();
	//禁止拷贝和赋值
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	//记录日志
	void log(const std::string& message);

	~Logger();
private:
	Logger();
	std::ofstream logFile;
	std::mutex mutex;
};
