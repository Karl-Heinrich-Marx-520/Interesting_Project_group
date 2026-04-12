#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

class Task {
public:
	int id;
	std::string description;
	int priority;
	std::string dueDate;

	std::string toString() const {
		std::ostringstream oss;
		oss << "ID:  " << id
			<< "  description:  " << description
			<< " priority:  " << priority
			<< "  DueDate:  " << dueDate;
		return oss.str();
	}
};
