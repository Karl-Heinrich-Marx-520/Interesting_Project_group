#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <fstream>
#include <atomic>
#include <sstream>
#include <vector>
#include <stdexcept>

// 辅助函数：将单个参数转换为字符串
template<typename T>
std::string to_string_helper(T&& arg) {
	std::ostringstream oss;
	oss << std::forward<T>(arg);
	return oss.str();
}
// 格式化字符串函数：将格式字符串中的 {} 替换为对应的参数值
//template<typename... Args>
//std::string format_string(const std::string& format, Args&&... args) {
//	std::vector<std::string> arg_strings = { to_string_helper(std::forward<Args>(args))... };
//	std::string result;
//	size_t arg_index = 0;
//	for (size_t i = 0; i < format.size(); ++i) {
//		if (format[i] == '{' && i + 1 < format.size() && format[i + 1] == '}') {
//			if (arg_index < arg_strings.size()) {
//				result += arg_strings[arg_index++];
//			} else {
//				result += "{}"; // 如果参数不足，保留占位符
//			}
//			++i; // 跳过 '}'
//		} else {
//			result += format[i];
//		}
//	}
//	return result;
//}

// LogQueue 类：线程安全的日志队列，支持多生产者和多消费者
class LogQueue {
public:
	void push(const std::string& msg) {
		std::lock_guard <std::mutex> lock(mutex_);
		queue_.push(msg);
		if (queue_.size() == 1) {
			cond_var_.notify_one();
			// 只有当队列从空变为非空时才通知消费者线程，避免不必要的唤醒，提高效率,依旧有可能存在虚假唤醒，但不会导致死锁，因为消费者线程会在唤醒后再次检查队列状态。
		}
	}
	bool pop(std::string& msg) {
		std::unique_lock<std::mutex> lock(mutex_);
		//while(queue_.empty() && !is_shutdown_) cond_var_.wait(lock); // C++11传统的条件变量使用方式
		cond_var_.wait(lock, [this]()->bool {return !queue_.empty() || is_shutdown_; }); //现代C++最佳实践
		//退出等待条件：队列非空或已关闭，优雅退出等待状态，避免虚假唤醒导致的死锁
		// 
//这个 if 是在唤醒后的二次校验
//场景：线程被 notify_one 唤醒，但在重新获取锁到执行 if 判断的间隙，其他消费者线程可能已经拿走了队首元素。
		if (is_shutdown_ && queue_.empty()) {
			return false; // 队列已关闭且没有消息可消费
		}

		msg = queue_.front();
		queue_.pop();
		return true;
	}
	void shutdown() {
		std::lock_guard<std::mutex> lock(mutex_);
		is_shutdown_ = true;
		cond_var_.notify_all(); // 唤醒所有等待的消费者线程，让它们能够检测到关闭状态并退出
	}

private:
	std::queue<std::string> queue_;
	std::mutex mutex_;
	std::condition_variable cond_var_;
	bool is_shutdown_ = false;
};


// Logger 类：日志记录器，负责将日志消息写入文件
class Logger {
public:
	Logger(const std::string& filename) : log_file_(filename, std::ios::out | std::ios::app), exit_flag_(false) {
		if (!log_file_.is_open()) {
			throw std::runtime_error("Faile to open log file");
		}

		// 启动消费者线程，负责从日志队列中取出日志消息并写入文件
		worker_thread_ = std::thread([this]()->void {
			std::string msg;
			while (log_queue_.pop(msg)) {
				log_file_ << msg << std::endl;
			}
		});
	}

	~Logger() {
		exit_flag_ = true;
		log_queue_.shutdown(); // 通知消费者线程退出
		if (worker_thread_.joinable()) {
			worker_thread_.join(); // 等待消费者线程结束
		}
		// 关闭日志文件
		if (log_file_.is_open()) {
			log_file_.close();
		}
	}

	template<typename... Args>
	void log(const std::string& format, Args&&... args) {
		log_queue_.push(formatMessage(format, std::forward<Args>(args)...));
	}

private:
	// 格式化日志消息，将格式字符串中的 {} 替换为对应的参数值
	template<typename... Args>
	std::string formatMessage(const std::string& format, Args&&... args) {
		std::vector<std::string> arg_strings = { to_string_helper(std::forward<Args>(args))... };
		std::ostringstream oss;
		size_t arg_index = 0;
		size_t pos = 0;
		size_t placeholder = format.find("{}", pos);
		while (placeholder != std::string::npos) {
			oss << format.substr(pos, placeholder - pos);
			if (arg_index < arg_strings.size()) {
				oss << arg_strings[arg_index++];
			}
			else {
				oss << "{}"; // 如果参数不足，保留占位符
			}
			pos = placeholder + 2; // 跳过 '{}'
			placeholder = format.find("{}", pos);
			
		}
		// 处理剩余的格式字符串和多余的参数
		oss << format.substr(pos); 
		while (arg_index < arg_strings.size()) {
			oss << arg_strings[arg_index++];
		}
		return oss.str();
	}
	LogQueue log_queue_; // 线程安全的日志队列，支持多生产者和多消费者
	std::thread worker_thread_; // 消费者线程，负责从日志队列中取出日志消息并写入文件
	std::ofstream log_file_; // 日志文件输出流，用于将日志消息写入文件
	std::atomic<bool> exit_flag_; // 退出标志，指示消费者线程何时退出，使用原子变量确保线程安全
};
