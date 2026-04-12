#pragma once
#include <iostream>
#include <string>
#include <memory>
#include "Logger.h"
#include "TaskManager.h"

//命令模式基类，使用CRTP实现静态多态
template <typename  Derived>
class Command{
public:
	void execute(const std::string& args) {
		static_cast<Derived*>(this)->executeImpl(args);
	}
};

class AddCommand : public Command<AddCommand> {
public:
	AddCommand(TaskManager& manager) : taskManager(manager) {}
	void executeImpl(const std::string& args) {
		size_t pos1 = args.find(',');
		size_t pos2 = args.find(',', pos1 + 1);
		if (pos1 == std::string::npos || pos2 == std::string::npos) {
			Logger::getInstance().log("Invalid arguments for add command. Expected format: description,priority,date");
			std::cout << "Error: Invalid arguments! Please enter in the format: description,priority,date" << std::endl;
			return;
		}

		std::string description = args.substr(0, pos1);
		int priority = std::stoi(args.substr(pos1 + 1, pos2 - pos1 - 1));
		std::string date = args.substr(pos2 + 1);
		taskManager.addTask(description, priority, date);
		std::cout << "Task added successfully." << std::endl;
	}

private:
	TaskManager& taskManager;
};

class DeleteCommand : public Command<DeleteCommand> {
public:
	DeleteCommand(TaskManager& manager) : taskManager(manager) {}
	void executeImpl(const std::string& args) {
		try {
			size_t pos;
			int id = std::stoi(args, &pos);
			if (pos != args.size()) {
				Logger::getInstance().log("Invalid arguments for delete command. Expected format: id");
				std::cout << "Error: Please enter numeric ID only!" << std::endl;
				return;
			}
			// 调用删除，并判断结果
			if (taskManager.deleteTask(id)) {
				std::cout << "Task deleted successfully." << std::endl;
			}
			else {
				std::cout << "Task with ID: " << id << " not found." << std::endl;
			}
		}
		// 捕获：根本不是数字（比如输入 delete abc）
		catch (const std::invalid_argument&) {
			Logger::getInstance().log("Invalid arguments for delete command. Expected format: id");
			std::cout << "Error: Invalid ID format!" << std::endl;
		}
		// 捕获：数字超出 int 范围
		catch (const std::out_of_range&) {
			Logger::getInstance().log("ID is out of range.");
			std::cout << "Error: ID out of range!" << std::endl;
		}
	}

private:
	TaskManager& taskManager;
};


class UpdateCommand : public Command<UpdateCommand> {
public:
	UpdateCommand(TaskManager& manager) : taskManager(manager) {}
	void executeImpl(const std::string& args) {
		size_t pos1 = args.find(',');
		size_t pos2 = args.find(',', pos1 + 1);
		size_t pos3 = args.find(',', pos2 + 1);
		if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) {
			Logger::getInstance().log("Invalid arguments for update command. Expected format: id,description,priority,date");
			std::cout << "Error: Invalid arguments! Please enter in the format: id,description,priority,date" << std::endl;
			return;
		}
		int id = std::stoi(args.substr(0, pos1));
		std::string description = args.substr(pos1 + 1, pos2 - pos1 - 1);
		int priority = std::stoi(args.substr(pos2 + 1, pos3 - pos2 - 1));
		std::string dueDate = args.substr(pos3 + 1);
		if (taskManager.updateTask(id, description, priority, dueDate)) {
			std::cout << "Task updated successfully." << std::endl;
		}
		else {
			std::cout << "Task with ID: " << id << " not found." << std::endl;
		}
	}
	
private:
	TaskManager& taskManager;
};

class ListCommand : public Command<ListCommand> {
public:
	ListCommand(TaskManager& manager) : taskManager(manager) {}
	void executeImpl(const std::string& args) {
		try {
			size_t pos = 0;
			int sortOption = std::stoi(args, &pos);
			if (pos != args.size() || sortOption < 0 || sortOption > 2) {
				Logger::getInstance().log("Invalid arguments for list command. Expected format: sortOption (0:ID, 1:Priority, 2:DueDate)");
				std::cout << "Error: Invalid sort option! Please enter 0, 1 or 2." << std::endl;
				return;
			}
			std::cout << "Listing tasks..." << std::endl;
			taskManager.listTasks(sortOption);
		}
		// 捕获：根本不是数字（比如输入 delete abc）
		catch (const std::invalid_argument&) {
			Logger::getInstance().log("Invalid arguments for delete command. Expected format: id");
			std::cout << "Error: Invalid ID format!" << std::endl;
		}
		// 捕获：数字超出 int 范围
		catch (const std::out_of_range&) {
			Logger::getInstance().log("ID is out of range.");
			std::cout << "Error: ID out of range!" << std::endl;
		}
	}

private:
	TaskManager& taskManager;
};


