#pragma once
#include "Task.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>

class TaskManager {
public:
	TaskManager();
	void addTask(const std::string& description, int priority, const std::string& date);
	bool deleteTask(int id);
	bool updateTask(int id, const std::string& description, int priority, const std::string& date);
	void listTasks(int sortOption) const; // 0:ID，1:优先级，2:截止日期
	void loadTasks();
	void saveTasks() const;

private:
	std::vector<Task> tasks;
	int nextId; //用于生成唯一ID
	static bool compareByPriority(const Task& a, const Task& b); //优先级高的排在前面
	static bool compareByDueDate(const Task& a, const Task& b); //截止日期早的排在前面
};
