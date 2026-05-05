#include <iostream>
#include <string>
#include <variant> // C++17 引入的 std::variant 用于存储不同类型的值
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <ostream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <stdexcept> // C++标准库中的异常类

//==========================================JsonValue 数据结构定义==========================================
class JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

class JsonValue {
private:
	std::variant<
		std::monostate, // null
		bool,           // true/false
		double,         // 数字
		std::string,    // 字符串
		JsonArray,      // 数组
		JsonObject      // 对象
	> m_value;

public:
//-----------------------构造函数--------------------------------

	JsonValue() : m_value(std::monostate{}) {}
	JsonValue(std::nullptr_t) : m_value(std::monostate{}) {}
	//bool类型
	JsonValue(bool b) : m_value(b) {}
	//数字类型
	JsonValue(double d) : m_value(d) {}
	JsonValue(int value) : m_value(static_cast<double>(value)) {} // 允许整数隐式转换为双精度
	//字符串类型
	JsonValue(const std::string& s) : m_value(s) {}
	JsonValue(std::string&& s) : m_value(std::move(s)) {}
	JsonValue(const char* s) : m_value(std::string(s)) {} // 允许C风格字符串隐式转换为std::string
	// 数组和对象类型
	JsonValue(const JsonArray& arr) : m_value(arr) {}
	JsonValue(const JsonObject& obj) : m_value(obj) {}

//------------------------类型检查和访问函数----------------------------

	bool is_null() const { return std::holds_alternative<std::monostate>(m_value); }
	bool is_bool() const { return std::holds_alternative<bool>(m_value); }
	bool is_number() const { return std::holds_alternative<double>(m_value); }
	bool is_string() const { return std::holds_alternative<std::string>(m_value); }
	bool is_array() const { return std::holds_alternative<JsonArray>(m_value); }
	bool is_object() const { return std::holds_alternative<JsonObject>(m_value); }
	// 获取值的引用，如果类型不匹配会抛出std::bad_variant_access异常
	bool as_bool() const {
		if (!is_bool()) throw std::runtime_error("类型错误：不是布尔值");
		return std::get<bool>(m_value);
	}
	double as_number() const {
		if (!is_number()) throw std::runtime_error("类型错误：不是数字");
		return std::get<double>(m_value);
	}
	//双版本的访问函数，const版本返回const引用，非const版本返回非常量引用，允许修改
	const std::string& as_string() const {
		if (!is_string()) throw std::runtime_error("类型错误：不是字符串");
		return std::get<std::string>(m_value);
	}
	std::string& as_string(){
		if (!is_string()) throw std::runtime_error("类型错误：不是字符串");
		return std::get<std::string>(m_value);
	}
	const JsonArray& as_array() const {
		if (!is_array()) throw std::runtime_error("类型错误：不是数组");
		return std::get<JsonArray>(m_value);
	}
	JsonArray& as_array() {
		if (!is_array()) throw std::runtime_error("当前值不是array类型，无法转换");
		return std::get<JsonArray>(m_value);
	}
	const JsonObject& as_object() const {
		if (!is_object()) throw std::runtime_error("类型错误：不是对象");
		return std::get<JsonObject>(m_value);
	}
	JsonObject& as_object() {
		if (!is_object()) throw std::runtime_error("当前值不是object类型，无法转换");
		return std::get<JsonObject>(m_value);
	}

//------------------------------------访问API：重载[]运算符--------------------------------------
	 // 数组用下标取值：json[0]
	JsonValue& operator[](size_t index) {
		return as_array()[index];
	}
	const JsonValue& operator[](size_t index) const {
		return as_array()[index];
	}
	// 对象用键取值：json["name"]
	JsonValue& operator[](const std::string& key) {
		return as_object()[key];
	}
	const JsonValue& operator[](const std::string& key) const {
		return as_object().at(key);
	}
	// 辅助函数：获取数组/对象的大小
	size_t size() const {
		if (is_array()) return as_array().size();
		if (is_object()) return as_object().size();
		throw std::runtime_error("只有数组和对象才能获取size");
	}

//------------------------------------迭代器接口--------------------------------------
	// 数组的迭代器，支持范围for循环
	JsonArray::iterator begin() { return as_array().begin(); }
	JsonArray::iterator end() { return as_array().end(); }
	JsonArray::const_iterator begin() const { return as_array().begin(); }
	JsonArray::const_iterator end() const { return as_array().end(); }

	// 对象的迭代器接口
	JsonObject::iterator object_begin() { return as_object().begin(); }
	JsonObject::iterator object_end() { return as_object().end(); }
	JsonObject::const_iterator object_begin() const { return as_object().begin(); }
	JsonObject::const_iterator object_end() const { return as_object().end(); }

//---------------------------------------序列化--------------------------------------
// indent: 缩进空格数(0=压缩,4=格式化)  current_indent: 当前递归层级
	std::string serialize(int indent = 0, int current_indent = 0) const {
		std::ostringstream oss;
		const std::string ind(indent * current_indent, ' '); // 当前层级的缩进
		const std::string ind_step(indent, ' '); // 每层的缩进增量

		//匹配variant类型
		std::visit([&](const auto& arg) {
			using T = std::decay_t<decltype(arg)>;

			// 1.Null
			if constexpr(std::is_same_v<T, std::monostate>)
				oss << "null";
			// 2.bool
			else if constexpr(std::is_same_v<T, bool>) 
				oss << (arg ? "true" : "false");
			// 3. 数字类型
			else if constexpr (std::is_same_v<T, double>) 
				serialize_number(oss, arg);
			// 4. 字符串类型
			else if constexpr (std::is_same_v<T, std::string>) 
				serialize_string(oss, arg);
			// 5.数组
			else if constexpr (std::is_same_v<T, JsonArray>)
				serialize_array(oss, arg, indent, current_indent, ind, ind_step);
			// 6.对象
			else if constexpr (std::is_same_v<T, JsonObject>)
				serialize_object(oss, arg, indent, current_indent, ind, ind_step);
			}, m_value);
		return oss.str();
	}

private:
	// 1. 字符串序列化
	void serialize_string(std::ostringstream& oss, const std::string& str) const {
		oss << '"';
		for (char c : str) {
			switch (c) {
			case '"':  oss << "\\\""; break; // 转义魔法哦~~~~
			case '\\': oss << "\\\\"; break;
			case '\b': oss << "\\b";  break;
			case '\f': oss << "\\f";  break;
			case '\n': oss << "\\n";  break;
			case '\r': oss << "\\r";  break;
			case '\t': oss << "\\t";  break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
				}
				else {
					oss << c;
				}
			}
		}
		oss << '"';
	}
	// 2. 数字序列化（整数/小数自动判断）
	void serialize_number(std::ostringstream& oss, double num) const {
		double int_part;
		if (std::modf(num, &int_part) == 0.0) {
			oss << static_cast<long long>(int_part);
		}
		else {
			oss << std::setprecision(15) << num;
		}
	}
	// 3. 数组序列化（递归+格式化缩进）
	void serialize_array(std::ostringstream& oss, const JsonArray& arr,
		int indent, int current_indent,
		const std::string& ind, const std::string& ind_step) const {
		oss << '[';
		if (indent > 0) oss << '\n';

		for (size_t i = 0; i < arr.size(); ++i) {
			if (indent > 0) oss << ind << ind_step;
			oss << arr[i].serialize(indent, current_indent + 1);
			if (i != arr.size() - 1) oss << ',';
			if (indent > 0) oss << '\n';
		}

		if (indent > 0) oss << ind;
		oss << ']';
	}
	// 4. 对象序列化（递归+键值对+格式化缩进）
	void serialize_object(std::ostringstream& oss, const JsonObject& obj,
		int indent, int current_indent,
		const std::string& ind, const std::string& ind_step) const {
		oss << '{';
		if (indent > 0) oss << '\n';

		size_t i = 0;
		for (const auto& [key, value] : obj) {
			if (indent > 0) oss << ind << ind_step;
			oss << '"' << key << "\":";
			if (indent > 0) oss << ' ';
			oss << value.serialize(indent, current_indent + 1);
			if (i != obj.size() - 1) oss << ',';
			if (indent > 0) oss << '\n';
			++i;
		}

		if (indent > 0) oss << ind;
		oss << '}';
	}
};

// =============================================Token 定义==============================================
// Token 类型枚举
enum class TokenType {
	EndOfInput,
	LeftBrace,    // {
	RightBrace,   // }
	LeftBracket,  // [
	RightBracket, // ]
	Comma,        // ,
	Colon,        // :
	String,       // 字符串
	Number,       // 数字
	True,         // true
	False,        // false
	Null          // null
};

// Token 结构体,包含类型、值和位置 (行和列，用来报错)
struct Token {
	TokenType type;
	std::string value;
	size_t line;
	size_t column;

	Token(TokenType t, const std::string& v, size_t l, size_t c) :
		type(t), value(v), line(l), column(c) {}
};



// ============================= 词法分析器Lexer核心类 =========================================
class Lexer {
private:
	std::string m_input; // 输入的 JSON 字符串
	size_t m_pos;        // 当前解析位置
	size_t m_line;       // 当前行号
	size_t m_column;     // 当前列号

public:
	Lexer(std::string input) : m_input(std::move(input)), m_pos(0), m_line(1), m_column(1) {}

	std::vector<Token> tokenize() {
		std::vector<Token> tokens; // 存储生成的 Token

		while (!is_at_end()) {
			skip_whitespace(); // 跳过空白字符
			if (is_at_end()) break; // 如果到达输入末尾，停止解析

			char current_char = peek(); // 获取当前字符
			size_t current_line = m_line; // 记录当前行号
			size_t current_column = m_column; // 记录当前列号

			switch (current_char) {
			case '{': advance(); tokens.emplace_back(TokenType::LeftBrace, "{", current_line, current_column); break;
			case '}': advance(); tokens.emplace_back(TokenType::RightBrace, "}", current_line, current_column); break;
			case '[': advance(); tokens.emplace_back(TokenType::LeftBracket, "[", current_line, current_column); break;
			case ']': advance(); tokens.emplace_back(TokenType::RightBracket, "]", current_line, current_column); break;
			case ',': advance(); tokens.emplace_back(TokenType::Comma, ",", current_line, current_column); break;
			case ':': advance(); tokens.emplace_back(TokenType::Colon, ":", current_line, current_column); break;
			// 遇到双引号，读取字符串
			case '"': tokens.push_back(read_string()); break;
			// 遇到关键字的开头，读取关键字
            case 't': tokens.push_back(read_keyword("true", TokenType::True)); break;
            case 'f': tokens.push_back(read_keyword("false", TokenType::False)); break;
            case 'n': tokens.push_back(read_keyword("null", TokenType::Null)); break;
			// 遇到数字或负号，读取数字
			case '-': case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': case '8': case '9':
				tokens.push_back(read_number());
				break;
			// 遇到不认识的字符，直接抛异常
			default:
				throw std::runtime_error(
					"词法分析错误：行" + std::to_string(m_line) +
					"列" + std::to_string(m_column) +
					"，意外字符：'" + current_char + "'"
				);
			}
		}
		// 最后加一个结束Token，告诉解析器字符串完了
		tokens.emplace_back(TokenType::EndOfInput, "", m_line, m_column);
		return tokens;
	}

private:
// ------------------------------------------------辅助工具函数----------------------------------
	//判断是否到达输入末尾
	bool is_at_end() const {
		return m_pos >= m_input.size();
	}
	//获取当前字符
	char peek() const {
		if (is_at_end()) return '\0'; // 返回空字符表示结束
		return m_input[m_pos];
	}
	//前进一个字符
	char advance() {
		char c = m_input[m_pos++];
		if (c == '\n') {
			m_line++;
			m_column = 1;
		} else {
			m_column++;
		}
		return c;
	}
	// 修改后的skip_whitespace，支持跳过注释
	void skip_whitespace() {
		while (!is_at_end()) {
			char c = peek();
			if (isspace(c)) {
				advance();
			}
			else if (c == '/') {
				// 单行注释 //
				if (peek_next() == '/') {
					advance(); advance();
					// 一直读到换行
					while (!is_at_end() && peek() != '\n') advance();
				}
				// 多行注释 /* */
				else if (peek_next() == '*') {
					advance(); advance();
					// 一直读到 */
					while (!is_at_end()) {
						if (peek() == '*' && peek_next() == '/') {
							advance(); advance();
							break;
						}
						advance();
					}
				}
				else {
					break;
				}
			}
			else {
				break;
			}
		}
	}

	// 查看下一个字符
	char peek_next() const {
		if (m_pos + 1 >= m_input.size()) return '\0';
		return m_input[m_pos + 1];
	}

//---------------------------------------读取函数---------------------------------------
	// 读取字符串：从开头的"读到结尾的"，处理转义字符
	Token read_string() {
		size_t start_line = m_line;
		size_t start_column = m_column;
		advance(); // 跳过开头的双引号
		std::string result;

		//
		while (!is_at_end() && peek() != '"') {
			char c = advance();
			if (c == '\\') { // 处理转义字符
				if (is_at_end()) {
					throw std::runtime_error(
						"词法分析错误：行" + std::to_string(start_line) +
						", 列 " + std::to_string(start_column) +
						"字符串未闭合，意外的转义结尾");
				}
				char esc_char = advance();
				switch (esc_char) {
					case '"': result += '"'; break;
					case '\\': result += '\\'; break;
                    case '/':  result += '/'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
					// 处理 \uXXXX 转义序列
					case 'u': {
						// 读取后面的4个十六进制字符
						if (m_column + 4 > m_input.size()) {
							throw std::runtime_error(
								"词法分析错误: 行" + std::to_string(start_line) +
								", 列" + std::to_string(start_column) +
								" 无效的 \\u 转义，缺少4位十六进制数");
						}
						std::string hex_str;
						for (int i = 0; i < 4; i++) {
							hex_str += advance();
						}
						// 十六进制字符串转成整数（0x0000 ~ 0xFFFF）
						unsigned int code;
						std::istringstream iss(hex_str);
						iss >> std::hex >> code;
						// 转成 char 加入结果（只处理ASCII控制字符，可扩展支持Unicode）
						result += static_cast<char>(code);
						break;
					}
					default:
						throw std::runtime_error(
							"词法分析错误：行" + std::to_string(start_line) +
							", 列 " + std::to_string(start_column) +
							"无效的转义字符 \\" + esc_char);
				}
			} else {
				result += c;
			}
		}
		if (is_at_end()) {
			throw std::runtime_error(
				"词法分析错误：行" + std::to_string(start_line) +
				", 列 " + std::to_string(start_column) +
				"字符串未闭合，意外的输入结尾");
		}
		advance(); // 跳过结尾的双引号
		return Token(TokenType::String, result, start_line, start_column);
	}

	// 读取关键字：true、false、null
	Token read_keyword(const std::string& keyword, TokenType type) {
		size_t start_line = m_line;
		size_t start_column = m_column;
		
		for (char c : keyword) {
			if(advance() != c){
				throw std::runtime_error(
                    "词法分析错误：行" + std::to_string(start_line) +
                    ", 列 " + std::to_string(start_column) +
                    "，无效的关键字，期望：" + keyword
                );
			}
		}
		return Token(type, keyword, start_line, start_column);
	}

	// 读取数字：整数或小数，支持负号
	Token read_number() {
		size_t start_line = m_line;
		size_t start_column = m_column;
		size_t start_pos = m_pos; // 记录数字开始的位置

		// 处理负号
		if (peek() == '-') advance();

		// 处理整数部分
		if (peek() == '0') {
			advance();
			if (isdigit(peek())) {
				throw std::runtime_error(
					"词法分析错误：行" + std::to_string(start_line) +
					"列" + std::to_string(start_column) +
					"，数字不能以0开头后跟其他数字"
				);
			}
		}
		else {
			while (isdigit(peek())) advance();
		}

		// 处理小数部分
		if (peek() == '.') {
			advance();
			if (!isdigit(peek())) {
				throw std::runtime_error(
					"词法分析错误：行" + std::to_string(m_line) +
					"列" + std::to_string(m_column) +
					"，小数点后必须跟数字"
				);
			}
		}
		while (isdigit(peek())) advance();

		// 截取数字的文本内容
		std::string num_str = m_input.substr(start_pos, m_pos - start_pos);
		return Token(TokenType::Number, num_str, start_line, start_column);
	}
};

// ===================== 语法分析器Parser核心类 =====================
class Parser {
private:
	std::vector<Token> m_tokens; // 词法分析器生成的Token列表
	size_t m_pos; // 当前解析位置

public:
	Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)), m_pos(0) {}

	// 解析入口函数，解析整个JSON文本，返回一个JsonValue
	JsonValue parse() {
		JsonValue root = parse_value(); // 从第一个值开始解析
		if(peek().type != TokenType::EndOfInput){
			throw std::runtime_error(
				"语法分析错误：行" + std::to_string(peek().line) +
				"列" + std::to_string(peek().column) +
				"，在JSON文本末尾发现多余的内容"
			);
		}
		return root;
	}

	// 静态辅助方法：一步到位，直接从字符串解析出JsonValue
	static JsonValue parse(const std::string& json_str) {
		Lexer lexer(json_str);
		auto tokens = lexer.tokenize();
		Parser parser(tokens);
		return parser.parse();
	}

	// 从文件读取JSON并解析
	static JsonValue parse_file(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("无法打开文件：" + filename);
		}
		std::ostringstream oss;
		oss << file.rdbuf(); // 把文件内容全部读入字符串
		return parse(oss.str()); // 调用parse方法解析
	}


private:
//---------------------------辅助工具函数-----------------------------

	const Token& peek() const {
		if (m_pos >= m_tokens.size()) {
			throw std::runtime_error("语法分析错误：意外的输入结尾");
		}
		return m_tokens[m_pos];
	}

	const Token& advance() {
		if (m_pos >= m_tokens.size()) {
			throw std::runtime_error("语法分析错误：意外的输入结尾");
		}
		return m_tokens[++m_pos];
	}
	// 匹配当前Token的类型，如果匹配成功，前进一个Token，返回true
	bool match(TokenType type) {
		if (peek().type == type) {
			advance();
			return true;
		}
		return false;
	}
	// 期望当前Token是指定类型，如果不是，直接抛异常
	void expect(TokenType type, const std::string& error_msg) {
		if (!match(type)) {
			throw std::runtime_error(
				"语法分析错误：行" + std::to_string(peek().line) +
				"列" + std::to_string(peek().column) +
				"，" + error_msg
			);
		}
	}

//--------------------------------解析函数----------------------------------
	// 解析任意JSON值：总入口
	JsonValue parse_value() {
		const Token& current_token = peek();
		switch (current_token.type) {
		case TokenType::Null:    advance(); return nullptr; // 解析null
		case TokenType::True:    advance(); return true;    // 解析true
		case TokenType::False:   advance(); return false;   // 解析false
		case TokenType::Number: { // 解析数字
			double num = std::stod(current_token.value); // 把字符串转成double
			advance();
			return num;
		}
		case TokenType::String: { // 解析字符串
			std::string str = current_token.value;
			advance();
			return str;
		}
		case TokenType::LeftBracket: return parse_array(); // 遇到[，解析数组
		case TokenType::LeftBrace:   return parse_object(); // 遇到{，解析对象
		default: // 遇到不认识的Token，抛异常
			throw std::runtime_error(
				"语法分析错误：行" + std::to_string(current_token.line) +
				"列" + std::to_string(current_token.column) +
				"，期望一个JSON值，遇到了意外的内容"
			);
		}
	}

	// 解析数组：[ 值1, 值2, ... ]
	JsonValue parse_array() {
		JsonArray arr;
		advance(); // 跳过左中括号 [

		// 如果直接遇到右中括号，说明是空数组
		if (match(TokenType::RightBracket)) {
			return arr;
		}

		// 循环解析数组里的每个元素
		while (true) {
			// 解析数组里的元素，每个元素都是一个JSON值
			arr.push_back(parse_value());

			// 遇到逗号，说明还有下一个元素，跳过逗号继续
			if (match(TokenType::Comma)) {
				// 支持尾随逗号：逗号后面直接是]，就结束，不报错
				if (peek().type == TokenType::RightBracket) break;
			}
			else {
				// 没有逗号，说明数组结束了
				break;
			}
		}

		// 数组必须以右中括号结尾，否则抛异常
		expect(TokenType::RightBracket, "数组未闭合，期望 ]");
		return arr;
	}

	JsonValue parse_object() {
		JsonObject obj;
		advance();

		if (match(TokenType::RightBrace)) {
			return obj;
		}

		while (true) {
			const Token& key_token = peek();
			if (key_token.type != TokenType::String) {
				throw std::runtime_error(
					"语法分析错误：行" + std::to_string(key_token.line) +
					"列" + std::to_string(key_token.column) +
					"，对象的键必须是字符串"
				);
			}

			std::string key = key_token.value;
			advance(); // 跳过键的字符串Token
			// 键后面必须跟冒号
			expect(TokenType::Colon, "键和值之间必须有冒号 :");
			// 解析冒号后面的值，值是任意JSON值
			obj[key] = parse_value();
			// 遇到逗号，说明还有下一个键值对
			if (match(TokenType::Comma)) {
				// 支持尾随逗号：逗号后面直接是}，就结束，不报错
				if (peek().type == TokenType::RightBrace) break;
			}
			else {
				// 没有逗号，说明对象结束了
				break;
			}
		}
		// 对象必须以右大括号结尾，否则抛异常
		expect(TokenType::RightBrace, "对象未闭合，期望 }");
		return obj;
	}
};


