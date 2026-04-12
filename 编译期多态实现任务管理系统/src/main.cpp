#include "Logger.h"
#include "TaskManager.h"
#include "Command.h"
#include <unordered_map>
#include <functional>
#include <variant>

int main() {
	TaskManager taskManager;
	//方式一：虚函数表，命令对象直接调用executeImpl方法，并不推荐，因为每个命令对象都需要继承同一个基类，并且基类需要定义虚函数，增加了运行时开销，那么CRTD就完全无用了
	// 同时也不够灵活，如果要添加新的命令类型，就需要修改现有的类层次结构
	// ListCommand listCommand(taskManager); 
	//std::unordered_map<std::string, std::unique_ptr<CommandBase>> commands;
	//commands["add"] = std::make_unique<AddCommand>(taskManager);
	//commands["delete"] = std::make_unique<DeleteCommand>(taskManager);
	//commands["update"] = std::make_unique<UpdateCommand>(taskManager);
	//commands["list"] = std::make_unique<ListCommand>(taskManager);

	 //方式二：CRTP，命令对象调用基类的execute方法，基类再调用派生类的executeImpl方法
	 //通过std::function和lambda表达式，将命令字符串映射到对应的命令对象的execute方法
	 //这样做的好处是：命令对象不需要继承同一个基类，也不需要虚函数表，减少了运行时开销
	 //同时也更灵活，可以随时添加新的命令类型，而不需要修改现有的类层次结构
	 //使用共享指针管理命令对象的生命周期，避免内存泄漏
	std::unordered_map<std::string, std::function<void(const std::string&)>> commands;
	auto add_command = std::make_shared<AddCommand>(taskManager);
	auto delete_command = std::make_shared<DeleteCommand>(taskManager);
	auto update_command = std::make_shared<UpdateCommand>(taskManager);
	auto list_command = std::make_shared<ListCommand>(taskManager);
	commands["add"] = [add_command](const std::string& args) ->void { 
		add_command->execute(args);
		};
	commands["delete"] = [delete_command](const std::string& args) ->void {
		delete_command->execute(args);
		};
	commands["update"] = [update_command](const std::string& args) ->void {
		update_command->execute(args);
		};
	commands["list"] = [list_command](const std::string& args) ->void {
		list_command->execute(args);
		};
	
	////方式三：std::variant
	//using CommandVariant = std::variant<
	//	std::unique_ptr<AddCommand>,
	//	std::unique_ptr<ListCommand>,
	//	std::unique_ptr<UpdateCommand>
	//>;

	//std::unordered_map<std::string, CommandVariant> commands;
	//commands["add"] = std::make_unique<AddCommand>(taskManager);
	//commands["delete"] = std::make_unique<DeleteCommand>(taskManager);
	//commands["update"] = std::make_unique<UpdateCommand>(taskManager);
	//commands["list"] = std::make_unique<ListCommand>(taskManager);

	std::cout << "Welcome to the Task Manager!" << std::endl;
	std::cout << "Available commands:  add,  delete,  list,  update,  exit" << std::endl;
	std::string input;
	while (true) {
		std::cout << "/n>";
		std::getline(std::cin, input);
		if (input.empty()) {
			continue;
		}
		size_t spacePos = input.find(' ');
		std::string command = input.substr(0, spacePos);
		std::string args;
		if(spacePos != std::string::npos) {
			args = input.substr(spacePos + 1);
		}
		if (command == "exit") {
			std::cout << "Exiting Task Manager. Goodbye!" << std::endl;
			break;
		}
		auto it = commands.find(command);
		if (it != commands.end()) {
			it->second(args);
		}
		else {
			std::cout << "Unknown command: " << command << std::endl;
			std::cout << "Available commands:  add,  delete,  list,  update,  exit" << std::endl;
		}
	}
	return 0;
}
