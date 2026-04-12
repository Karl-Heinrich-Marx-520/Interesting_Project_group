#include "Task.h"
#include "Logger.h"
#include "TaskManager.h"


TaskManager::TaskManager() : nextId(1) {
	loadTasks();
}

//加载任务列表，构造函数中调用loadTasks方法，从文件中读取任务数据并初始化任务列表和下一个ID
void TaskManager::loadTasks() {
	std::ifstream inFile("tasks.txt");
	if(!inFile.is_open()) {
		Logger::getInstance().log("Failed to open tasks.txt for reading");
		return;
	}
	std::string line;
	while (std::getline(inFile, line)) {
		std::istringstream iss(line); //注释方法因为std::getline 返回的是 std::istream&，无法直接赋值给整数或其他类型
		Task task;
		char delimiter; //分隔符
		////两种方式读取数据：使用逗号分隔，或者直接读取
		//iss >> task.id >> delimiter; //第一种，读取ID，直到逗号为止，逗号进入delimiter
		//std::getline(iss, task.description, ','); //第二种，直接读取描述，直到逗号为止，逗号被丢弃
		//iss >> task.priority >> delimiter;
		//std::getline(iss, task.dueDate);
		//完全流方法
		iss >> task.id >> delimiter
			>> task.description >> delimiter
			>> task.priority >> delimiter
			>> task.dueDate;
		tasks.push_back(task);
		if (task.id >= nextId) {
			nextId = task.id + 1;
		}
	}
	inFile.close();
	Logger::getInstance().log("Tasks loaded successfully");
}

void TaskManager::addTask(const std::string& description, int priority, const std::string& date) {
	Task task;
	task.id = nextId++;
	task.description = description;
	task.priority = priority;
	task.dueDate = date;
	tasks.push_back(task);
	Logger::getInstance().log("Added task: " + task.toString());
	saveTasks();
}

bool TaskManager::deleteTask(int id) {
	auto it = std::find_if(tasks.begin(), tasks.end(), [id](const Task& task) {
		return task.id == id;
		});
	if (it != tasks.end()) {
		tasks.erase(it);
		Logger::getInstance().log("Deleted task with ID: " + std::to_string(id));
		saveTasks(); //删除后保存任务列表
		return true;
	}
	else {
		Logger::getInstance().log("Task with ID: " + std::to_string(id) + " not found");
		return false;
	}
}

bool  TaskManager::updateTask(int id, const std::string& description, int priority, const std::string& date) {
	auto it = std::find_if(tasks.begin(), tasks.end(), [id](const Task& task) ->bool {
		return task.id == id;
		});
	if (it != tasks.end()) {
		it->description = description;
		it->priority = priority;
		it->dueDate = date;
		Logger::getInstance().log("Updated task: " + it->toString());
		saveTasks(); //更新后保存任务列表
		return true;
	}
	else {
		Logger::getInstance().log("Task with ID: " + std::to_string(id) + " not found");
		return false;
	}
}

//保存任务列表，saveTasks方法将当前任务列表写入文件，每个任务占一行，字段之间用逗号分隔
void TaskManager::saveTasks() const {
	std::ofstream outFile("tasks.txt"); //std::ofstream 打开文件时，默认ios::out模式，会把旧内容全部清空
	if (!outFile.is_open()) {
		Logger::getInstance().log("Failed to open tasks.txt for writing");
		return;
	}

	for (const auto& task : tasks) {
		outFile << task.id << "," 
			<< task.description << "," 
			<< task.priority << "," 
			<< task.dueDate << std::endl;
	}
	outFile.close();
	Logger::getInstance().log("Tasks saved successfully");
}

void TaskManager::listTasks(int sortOption) const {
	std::vector<Task> sortedTasks = tasks; //创建一个副本用于排序
	switch (sortOption) {
	case 1:
		std::sort(sortedTasks.begin(), sortedTasks.end(), compareByPriority); break;
	case 2:
		std::sort(sortedTasks.begin(), sortedTasks.end(), compareByDueDate); break;
	default:
		std::sort(sortedTasks.begin(), sortedTasks.end(), [](const Task& a, const Task& b) {
			return a.id < b.id; //默认按ID排序
			});
	}

	for (const auto& task : sortedTasks) {
		std::cout << task.toString() << std::endl;
	}
}

bool TaskManager::compareByPriority(const Task& a, const Task& b) {
	return a.priority > b.priority; //优先级高的排在前面
}

bool TaskManager::compareByDueDate(const Task& a, const Task& b) {
	return a.dueDate < b.dueDate; //截止日期早的排在前面
}
