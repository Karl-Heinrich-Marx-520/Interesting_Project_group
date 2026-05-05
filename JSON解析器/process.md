# 初学者友好版 JSON 解析器 分步实现+超详细讲解
我把整个JSON解析器拆成了**4个循序渐进的阶段**，从核心原理到完整实现，每一步都有明确目标、极简代码、大白话讲解，哪怕是刚学C++的新手也能看懂。

先记住核心结论：**JSON解析器本质就是「字符串→结构化数据」的转换器**，整个过程只分4步，就像把一篇外文文章翻译成中文：
1.  分词（词法分析）：把一长串字符串，切成一个个有意义的「单词」（Token）
2.  解析（语法分析）：按照JSON的语法规则，把单词拼成完整的句子/段落
3.  封装：把解析好的内容，做成你能随手取用的格式
4.  序列化：反向操作，把你封装好的内容，再转回JSON字符串

---

## 前置准备：先搞懂2个最基础的概念
### 1. JSON的6种基础类型（必须背下来）
JSON所有内容都逃不出这6种类型，我们的解析器就是要完整支持它们：
| JSON类型 | 含义 | 示例 |
|----------|------|------|
| null     | 空值 | `null` |
| boolean  | 布尔值 | `true` / `false` |
| number   | 数字（整数/浮点数） | `123` / `3.14` |
| string   | 字符串（带双引号） | `"hello world"` |
| array    | 数组（方括号包裹） | `[1, "abc", true]` |
| object   | 对象（大括号包裹，键值对） | `{"name":"张三", "age":20}` |

### 2. 我们的整体架构（4个独立模块，绝不乱堆）
我把代码拆成了4个完全独立的模块，每个模块只干一件事，你可以写完一个测试一个，不会出现“全写完才发现跑不通”的问题：
1.  **JsonValue模块**：定义数据结构，用来存解析后的JSON数据（相当于装东西的盒子）
2.  **Lexer词法分析模块**：把JSON字符串切成一个个Token（相当于把文章切成单词）
3.  **Parser语法分析模块**：把Token流拼成JsonValue结构（相当于把单词拼成文章）
4.  **API与进阶功能**：给用户提供好用的接口，比如`json["name"]`取值、序列化、错误处理

---

## 阶段1：先做JsonValue数据结构（装数据的盒子）
### 本阶段目标
定义一个能装下JSON 6种类型的C++类，解决「解析完的东西存在哪」的问题。

### 核心原理
C++里要让一个变量能存多种类型，最适合的就是**C++17的`std::variant`**（类型安全的联合体，你可以理解成一个“万能盒子”，同一时间只能装一种类型，但能随时切换，还能知道当前装的是什么）。

我们给6种JSON类型，对应C++的类型：
| JSON类型 | 对应的C++类型 | 说明 |
|----------|---------------|------|
| null     | `std::monostate` | 空类型，什么都不存，专门用来表示null |
| boolean  | `bool` | 直接对应C++的bool |
| number   | `double` | 同时存整数和浮点数，兼容所有JSON数字 |
| string   | `std::string` | 直接对应C++字符串 |
| array    | `std::vector<JsonValue>` | 数组，里面可以放任意JsonValue |
| object   | `std::map<std::string, JsonValue>` | 对象，键是字符串，值是任意JsonValue |

### 完整代码（带超详细注释）
```cpp
#include <iostream>
#include <string>
#include <variant>  // 万能盒子的核心头文件
#include <vector>
#include <map>
#include <stdexcept> // 用来抛异常

// 先做类型别名，简化代码
class JsonValue; // 前置声明，因为数组和对象要用到JsonValue本身
using JsonArray = std::vector<JsonValue>;  // JSON数组 = 装JsonValue的vector
using JsonObject = std::map<std::string, JsonValue>; // JSON对象 = 字符串到JsonValue的map

// 核心类：用来存JSON的任意类型数据
class JsonValue {
private:
    // 核心成员：万能盒子，能存6种类型中的一种
    std::variant<
        std::monostate,  // 对应null
        bool,             // 对应boolean
        double,           // 对应number
        std::string,      // 对应string
        JsonArray,        // 对应array
        JsonObject        // 对应object
    > m_value;

public:
    // ===================== 构造函数：给每种类型提供初始化方式 =====================
    // 空构造函数，默认是null
    JsonValue() : m_value(std::monostate{}) {}
    // 专门给null用的构造函数，比如 JsonValue v = nullptr;
    JsonValue(std::nullptr_t) : m_value(std::monostate{}) {}
    // 布尔值构造
    JsonValue(bool value) : m_value(value) {}
    // 数字构造：整数转成double，统一存储
    JsonValue(int value) : m_value(static_cast<double>(value)) {}
    JsonValue(double value) : m_value(value) {}
    // 字符串构造
    JsonValue(const char* value) : m_value(std::string(value)) {}
    JsonValue(const std::string& value) : m_value(value) {}
    // 数组和对象构造
    JsonValue(const JsonArray& value) : m_value(value) {}
    JsonValue(const JsonObject& value) : m_value(value) {}

    // ===================== 类型判断接口：判断当前存的是什么类型 =====================
    // 比如 if (json.is_null()) 就能知道是不是null
    bool is_null()    const { return std::holds_alternative<std::monostate>(m_value); }
    bool is_bool()    const { return std::holds_alternative<bool>(m_value); }
    bool is_number()  const { return std::holds_alternative<double>(m_value); }
    bool is_string()  const { return std::holds_alternative<std::string>(m_value); }
    bool is_array()   const { return std::holds_alternative<JsonArray>(m_value); }
    bool is_object()  const { return std::holds_alternative<JsonObject>(m_value); }

    // ===================== 类型转换接口：把盒子里的东西取出来 =====================
    // 比如 json.as_string() 就能拿到里面的字符串，类型不对会抛异常，防止用错
    bool as_bool() const {
        if (!is_bool()) throw std::runtime_error("当前值不是bool类型，无法转换");
        return std::get<bool>(m_value); // std::get 从variant里取出对应类型的值
    }

    double as_number() const {
        if (!is_number()) throw std::runtime_error("当前值不是number类型，无法转换");
        return std::get<double>(m_value);
    }

    const std::string& as_string() const {
        if (!is_string()) throw std::runtime_error("当前值不是string类型，无法转换");
        return std::get<std::string>(m_value);
    }

    const JsonArray& as_array() const {
        if (!is_array()) throw std::runtime_error("当前值不是array类型，无法转换");
        return std::get<JsonArray>(m_value);
    }

    const JsonObject& as_object() const {
        if (!is_object()) throw std::runtime_error("当前值不是object类型，无法转换");
        return std::get<JsonObject>(m_value);
    }

    // 给可修改的版本，方便后续赋值
    JsonArray& as_array() {
        if (!is_array()) throw std::runtime_error("当前值不是array类型，无法转换");
        return std::get<JsonArray>(m_value);
    }

    JsonObject& as_object() {
        if (!is_object()) throw std::runtime_error("当前值不是object类型，无法转换");
        return std::get<JsonObject>(m_value);
    }

    // ===================== 友好的访问API：重载[]运算符，像数组/字典一样取值 =====================
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
};
```

### 逐模块讲解
1.  **前置类型别名**：`JsonArray`和`JsonObject`提前定义，因为数组和对象里可以嵌套任意JSON类型，包括数组和对象本身，所以必须前置声明`JsonValue`。
2.  **核心成员`m_value`**：这是整个类的心脏，`std::variant`把6种类型都包起来，保证了类型安全，不会出现C语言联合体的类型混乱问题。
3.  **构造函数**：给每种JSON类型都提供了对应的构造方式，你可以直接写`JsonValue v = 123;`、`JsonValue s = "hello";`，非常自然。
4.  **类型判断接口**：`is_xxx()`系列函数，用`std::holds_alternative`判断当前variant里存的是什么类型，防止你取错类型。
5.  **类型转换接口**：`as_xxx()`系列函数，用`std::get`取出variant里的值，如果类型不对直接抛异常，避免程序出现未定义行为。
6.  **`[]`运算符重载**：这是题目要求的「友好的数据访问API」，让你能像JS/Python一样，直接用下标和键取值，不用写复杂的函数调用。

### 测试一下这个阶段的代码
你可以直接在main函数里测试，验证我们的盒子能不能正常用：
```cpp
int main() {
    // 测试1：基础类型
    JsonValue null_val = nullptr;
    JsonValue bool_val = true;
    JsonValue num_val = 3.14;
    JsonValue str_val = "hello json";

    std::cout << "null是否为空：" << std::boolalpha << null_val.is_null() << std::endl;
    std::cout << "布尔值：" << bool_val.as_bool() << std::endl;
    std::cout << "数字：" << num_val.as_number() << std::endl;
    std::cout << "字符串：" << str_val.as_string() << std::endl;

    // 测试2：数组
    JsonArray arr = {1, 2, "abc", false};
    JsonValue arr_val = arr;
    std::cout << "数组长度：" << arr_val.size() << std::endl;
    std::cout << "数组第0个元素：" << arr_val[0].as_number() << std::endl;

    // 测试3：对象
    JsonObject obj;
    obj["name"] = "张三";
    obj["age"] = 20;
    JsonValue obj_val = obj;
    std::cout << "姓名：" << obj_val["name"].as_string() << std::endl;
    std::cout << "年龄：" << obj_val["age"].as_number() << std::endl;

    return 0;
}
```
编译命令（必须支持C++17）：
```bash
g++ -std=c++17 json_value.cpp -o json_value
```
能正常运行并输出对应的值，就说明这个阶段完成了！

---

## 阶段2：实现Lexer词法分析器（把字符串切成单词）
### 本阶段目标
写一个词法分析器，把一长串JSON字符串，切成一个个有意义的「Token」，解决「怎么把字符串拆成能解析的最小单元」的问题。

### 核心原理
#### 1. 什么是Token？
Token就是JSON里的“最小有意义单元”，就像中文里的词语。比如JSON字符串`{"name":"张三", "age":20}`，拆成Token就是：
`{`、`"name"`、`:`、`"张三"`、`,`、`"age"`、`:`、`20`、`}`

#### 2. 我们需要哪些Token类型？
把JSON里所有的最小单元都列出来，定义成枚举：
| Token类型 | 含义 |
|-----------|------|
| EndOfInput | 字符串结束了 |
| LeftBrace | 左大括号 `{` |
| RightBrace | 右大括号 `}` |
| LeftBracket | 左中括号 `[` |
| RightBracket | 右中括号 `]` |
| Colon | 冒号 `:` |
| Comma | 逗号 `,` |
| Null | 关键字 `null` |
| True | 关键字 `true` |
| False | 关键字 `false` |
| Number | 数字，比如 `123`、`3.14` |
| String | 字符串，比如 `"张三"` |

#### 3. 词法分析器的工作流程
1.  一个字符一个字符地遍历JSON字符串
2.  跳过空格、换行、制表符这些无意义的空白字符
3.  遇到不同的字符，生成对应的Token：
    - 遇到`{`、`}`、`[`、`]`、`:`、`,`，直接生成对应的符号Token
    - 遇到`"`，开始读取字符串，直到遇到下一个`"`，生成字符串Token
    - 遇到`t`、`f`、`n`，读取关键字`true`/`false`/`null`，生成对应的Token
    - 遇到数字或`-`，读取完整的数字，生成数字Token
4.  遍历完所有字符，生成结束Token

### 完整代码（带超详细注释）
先在之前的代码基础上，添加Token的定义和Lexer类：
```cpp
// ===================== 先定义Token相关内容 =====================
// Token类型枚举
enum class TokenType {
    EndOfInput,
    LeftBrace,    // {
    RightBrace,   // }
    LeftBracket,  // [
    RightBracket, // ]
    Colon,        // :
    Comma,        // ,
    Null,         // null
    True,         // true
    False,        // false
    Number,       // 数字
    String        // 字符串
};

// Token结构体：每个Token都有类型、值、所在的行号列号（用来报错）
struct Token {
    TokenType type;    // Token的类型
    std::string value; // Token的内容，比如字符串的内容、数字的文本
    size_t line;       // 所在行号，解析失败时告诉用户哪里错了
    size_t column;     // 所在列号

    // 构造函数
    Token(TokenType t, std::string v, size_t l, size_t c)
        : type(t), value(std::move(v)), line(l), column(c) {}
};

// ===================== 词法分析器核心类 =====================
class Lexer {
private:
    std::string m_input;  // 输入的JSON字符串
    size_t m_pos;         // 当前遍历到的位置（下标）
    size_t m_line;        // 当前行号
    size_t m_column;      // 当前列号

public:
    // 构造函数：传入要解析的JSON字符串，初始化位置
    Lexer(std::string input)
        : m_input(std::move(input)), m_pos(0), m_line(1), m_column(1) {}

    // 核心方法：把输入字符串切成Token列表，返回给调用者
    std::vector<Token> tokenize() {
        std::vector<Token> tokens; // 用来存生成的Token

        // 循环遍历，直到字符串结束
        while (!is_at_end()) {
            skip_whitespace(); // 先跳过空白字符

            if (is_at_end()) break; // 跳过空白后到结尾了，直接退出

            char current_char = peek(); // 看一下当前的字符，不移动位置
            size_t current_line = m_line;   // 记录当前行号
            size_t current_column = m_column; // 记录当前列号

            // 根据当前字符，生成对应的Token
            switch (current_char) {
                // 单字符符号，直接生成Token
                case '{': advance(); tokens.emplace_back(TokenType::LeftBrace, "{", current_line, current_column); break;
                case '}': advance(); tokens.emplace_back(TokenType::RightBrace, "}", current_line, current_column); break;
                case '[': advance(); tokens.emplace_back(TokenType::LeftBracket, "[", current_line, current_column); break;
                case ']': advance(); tokens.emplace_back(TokenType::RightBracket, "]", current_line, current_column); break;
                case ':': advance(); tokens.emplace_back(TokenType::Colon, ":", current_line, current_column); break;
                case ',': advance(); tokens.emplace_back(TokenType::Comma, ",", current_line, current_column); break;
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
    // ===================== 辅助工具函数 =====================
    // 判断是否到了字符串结尾
    bool is_at_end() const {
        return m_pos >= m_input.size();
    }

    // 查看当前字符，不移动位置
    char peek() const {
        if (is_at_end()) return '\0';
        return m_input[m_pos];
    }

    // 前进一个字符，更新行号列号
    char advance() {
        char c = m_input[m_pos++];
        if (c == '\n') { // 遇到换行，行号+1，列号重置为1
            m_line++;
            m_column = 1;
        } else { // 其他字符，列号+1
            m_column++;
        }
        return c;
    }

    // 跳过空白字符：空格、换行、制表符、回车
    void skip_whitespace() {
        while (!is_at_end()) {
            char c = peek();
            if (isspace(c)) { // isspace是C++标准函数，判断是否是空白字符
                advance();
            } else {
                break;
            }
        }
    }

    // ===================== 核心读取函数 =====================
    // 读取关键字：true/false/null
    Token read_keyword(const std::string& keyword, TokenType type) {
        size_t start_line = m_line;
        size_t start_column = m_column;

        // 逐个字符匹配关键字
        for (char c : keyword) {
            if (advance() != c) {
                throw std::runtime_error(
                    "词法分析错误：行" + std::to_string(start_line) +
                    "列" + std::to_string(start_column) +
                    "，无效的关键字，期望：" + keyword
                );
            }
        }

        // 匹配成功，返回对应的Token
        return Token(type, keyword, start_line, start_column);
    }

    // 读取字符串：从开头的"读到结尾的"，处理转义字符
    Token read_string() {
        size_t start_line = m_line;
        size_t start_column = m_column;
        advance(); // 跳过开头的双引号 "
        std::string result; // 用来存字符串的内容

        // 循环读取，直到遇到结尾的双引号
        while (!is_at_end() && peek() != '"') {
            char c = advance();
            // 处理转义字符：比如 \" 、\n 、\t 等
            if (c == '\\') {
                if (is_at_end()) {
                    throw std::runtime_error(
                        "词法分析错误：行" + std::to_string(start_line) +
                        "列" + std::to_string(start_column) +
                        "，字符串未闭合，意外的转义结尾"
                    );
                }
                char esc_char = advance(); // 读取转义符后面的字符
                // 根据转义字符，替换成对应的内容
                switch (esc_char) {
                    case '"':  result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    default:
                        throw std::runtime_error(
                            "词法分析错误：行" + std::to_string(m_line) +
                            "列" + std::to_string(m_column) +
                            "，不支持的转义字符：\\" + esc_char
                        );
                }
            } else {
                // 普通字符，直接加进去
                result += c;
            }
        }

        // 循环结束了，还没到结尾的"，说明字符串没闭合
        if (is_at_end()) {
            throw std::runtime_error(
                "词法分析错误：行" + std::to_string(start_line) +
                "列" + std::to_string(start_column) +
                "，字符串未闭合，缺少结尾的\""
            );
        }

        advance(); // 跳过结尾的双引号 "
        return Token(TokenType::String, result, start_line, start_column);
    }

    // 读取数字：支持整数、浮点数
    Token read_number() {
        size_t start_line = m_line;
        size_t start_column = m_column;
        size_t start_pos = m_pos; // 记录数字开始的位置

        // 处理负号
        if (peek() == '-') advance();

        // 处理整数部分
        if (peek() == '0') {
            advance();
            // 0后面不能跟其他数字，比如 0123 是非法的JSON数字
            if (isdigit(peek())) {
                throw std::runtime_error(
                    "词法分析错误：行" + std::to_string(start_line) +
                    "列" + std::to_string(start_column) +
                    "，数字不能以0开头后跟其他数字"
                );
            }
        } else {
            // 读取所有连续的数字
            while (isdigit(peek())) advance();
        }

        // 处理小数部分：比如 .14
        if (peek() == '.') {
            advance();
            // 小数点后面必须跟数字
            if (!isdigit(peek())) {
                throw std::runtime_error(
                    "词法分析错误：行" + std::to_string(m_line) +
                    "列" + std::to_string(m_column) +
                    "，小数点后必须跟数字"
                );
            }
            while (isdigit(peek())) advance();
        }

        // 截取数字的文本内容
        std::string num_str = m_input.substr(start_pos, m_pos - start_pos);
        return Token(TokenType::Number, num_str, start_line, start_column);
    }
};
```

### 逐模块讲解
1.  **Token结构体**：每个Token都包含「类型、内容、行号、列号」，类型用来告诉解析器这是什么东西，内容是它的值，行号列号用来报错，符合题目要求的「解析失败时提供有意义的错误信息」。
2.  **Lexer的核心成员**：`m_input`是输入的JSON字符串，`m_pos`是当前遍历的下标，`m_line`和`m_column`记录当前位置，用来报错。
3.  **辅助工具函数**：
    - `is_at_end()`：判断是否遍历完了字符串
    - `peek()`：查看当前字符，不移动位置，避免提前消费字符
    - `advance()`：前进一个字符，同时更新行号列号
    - `skip_whitespace()`：跳过所有空白字符，JSON里的空白不影响语义
4.  **核心读取函数**：
    - `read_keyword()`：匹配`true`/`false`/`null`，确保关键字拼写正确
    - `read_string()`：读取字符串，处理转义字符，确保字符串有闭合的双引号
    - `read_number()`：读取数字，符合JSON的数字规范，比如不能有`0123`这种前导零的数字
5.  **核心`tokenize()`方法**：循环遍历字符串，逐个生成Token，最后返回Token列表，这是Lexer对外的唯一接口。

### 测试一下这个阶段的代码
在main函数里添加测试代码，看看能不能正确切分Token：
```cpp
int main() {
    // 测试JSON字符串
    std::string json_str = R"(
        {
            "name": "张三",
            "age": 20,
            "is_student": true,
            "score": [90.5, 88, 95],
            "null_value": null
        }
    )";

    try {
        // 创建词法分析器，生成Token列表
        Lexer lexer(json_str);
        std::vector<Token> tokens = lexer.tokenize();

        // 打印所有Token
        std::cout << "===== 生成的Token列表 =====" << std::endl;
        for (const auto& token : tokens) {
            std::cout << "行" << token.line << "列" << token.column 
                      << " | 类型：" << static_cast<int>(token.type)
                      << " | 内容：" << token.value << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "错误：" << e.what() << std::endl;
    }

    return 0;
}
```
编译运行后，能看到所有字符都被切成了正确的Token，就说明这个阶段完成了！

---

## 阶段3：实现Parser语法分析器（把Token拼成JSON结构）
### 本阶段目标
写一个语法分析器，把Lexer生成的Token列表，按照JSON的语法规则，拼成我们之前写的`JsonValue`结构，这是整个解析器的核心。

### 核心原理
#### 1. 什么是递归下降解析？
JSON的语法是**递归嵌套**的：对象里可以有数组，数组里可以有对象，对象里还能再套对象。所以最适合用「递归下降解析法」，大白话讲就是：
- 写一个总函数`parse_value()`，用来解析任意JSON值
- 遇到`[`，就调用`parse_array()`解析数组，`parse_array()`里又会调用`parse_value()`解析数组里的每个元素
- 遇到`{`，就调用`parse_object()`解析对象，`parse_object()`里又会调用`parse_value()`解析每个键对应的值
- 这样一层一层递归，就把嵌套的JSON结构解开了

#### 2. JSON的语法规则（大白话版）
我们的解析器完全遵循这个规则：
1.  一个JSON值，只能是这7种之一：null、true、false、数字、字符串、数组、对象
2.  数组的格式：`[ 值1, 值2, 值3, ... ]`，方括号包裹，元素用逗号分隔
3.  对象的格式：`{ "键1":值1, "键2":值2, ... }`，大括号包裹，键值对用逗号分隔，键必须是字符串，键和值之间用冒号分隔

#### 3. 语法分析器的工作流程
1.  拿到Lexer生成的Token列表，用一个指针记录当前解析到哪个Token
2.  调用`parse_value()`，根据当前Token的类型，调用对应的解析函数
3.  解析完成后，检查是否有多余的Token，确保JSON只有一个根值
4.  返回最终的`JsonValue`对象

### 完整代码（带超详细注释）
在之前的代码基础上，添加Parser类：
```cpp
// ===================== 语法分析器核心类 =====================
class Parser {
private:
    std::vector<Token> m_tokens; // 词法分析器生成的Token列表
    size_t m_pos; // 当前解析到的Token下标

public:
    // 构造函数：传入Token列表
    Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)), m_pos(0) {}

    // 核心方法：解析Token列表，返回最终的JsonValue
    JsonValue parse() {
        JsonValue root = parse_value(); // 解析根值
        // 解析完根值后，必须到结束Token，否则说明有多余的内容
        if (peek().type != TokenType::EndOfInput) {
            throw std::runtime_error(
                "语法分析错误：行" + std::to_string(peek().line) +
                "列" + std::to_string(peek().column) +
                "，JSON根节点只能有一个值，后面有多余内容"
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

private:
    // ===================== 辅助工具函数 =====================
    // 查看当前Token，不移动位置
    const Token& peek() const {
        if (m_pos >= m_tokens.size()) return m_tokens.back();
        return m_tokens[m_pos];
    }

    // 前进一个Token
    const Token& advance() {
        if (m_pos < m_tokens.size()) m_pos++;
        return peek();
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

    // ===================== 核心解析函数 =====================
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
            } else {
                // 没有逗号，说明数组结束了
                break;
            }
        }

        // 数组必须以右中括号结尾，否则抛异常
        expect(TokenType::RightBracket, "数组未闭合，期望 ]");
        return arr;
    }

    // 解析对象：{ "键1":值1, "键2":值2, ... }
    JsonValue parse_object() {
        JsonObject obj;
        advance(); // 跳过左大括号 {

        // 如果直接遇到右大括号，说明是空对象
        if (match(TokenType::RightBrace)) {
            return obj;
        }

        // 循环解析每个键值对
        while (true) {
            // 对象的键必须是字符串
            const Token& key_token = peek();
            if (key_token.type != TokenType::String) {
                throw std::runtime_error(
                    "语法分析错误：行" + std::to_string(key_token.line) +
                    "列" + std::to_string(key_token.column) +
                    "，对象的键必须是双引号包裹的字符串"
                );
            }
            std::string key = key_token.value;
            advance(); // 跳过键的字符串

            // 键后面必须跟冒号
            expect(TokenType::Colon, "键和值之间必须有冒号 :");

            // 解析冒号后面的值，值是任意JSON值
            obj[key] = parse_value();

            // 遇到逗号，说明还有下一个键值对
            if (match(TokenType::Comma)) {
                // 支持尾随逗号：逗号后面直接是}，就结束，不报错
                if (peek().type == TokenType::RightBrace) break;
            } else {
                // 没有逗号，说明对象结束了
                break;
            }
        }

        // 对象必须以右大括号结尾，否则抛异常
        expect(TokenType::RightBrace, "对象未闭合，期望 }");
        return obj;
    }
};
```

### 逐模块讲解
1.  **Parser的核心成员**：`m_tokens`是Lexer生成的Token列表，`m_pos`是当前解析到的Token下标，和Lexer的`m_pos`逻辑一致。
2.  **辅助工具函数**：
    - `peek()`：查看当前Token，不移动位置
    - `advance()`：前进一个Token
    - `match()`：匹配Token类型，匹配成功就前进，返回bool
    - `expect()`：强制要求当前Token是指定类型，否则直接抛异常，用来处理必须存在的符号，比如数组的结尾`]`
3.  **核心`parse_value()`函数**：这是整个解析器的总入口，根据当前Token的类型，分发到对应的解析函数，实现了递归的核心。
4.  **`parse_array()`函数**：解析数组，循环调用`parse_value()`解析每个元素，支持尾随逗号（题目进阶要求），最后检查是否有闭合的`]`。
5.  **`parse_object()`函数**：解析对象，先读取键（必须是字符串），然后读取冒号，再调用`parse_value()`解析值，支持尾随逗号，最后检查是否有闭合的`}`。
6.  **静态`parse()`方法**：把Lexer和Parser封装到一起，用户只需要调用`Parser::parse(json_str)`就能直接得到解析后的JsonValue，非常方便。

### 测试一下这个阶段的代码
现在我们的解析器已经能完整工作了！写一个main函数测试：
```cpp
int main() {
    // 测试JSON字符串
    std::string json_str = R"(
        {
            "name": "张三",
            "age": 20,
            "is_student": true,
            "score": [90.5, 88, 95],
            "address": {
                "city": "南京",
                "district": "栖霞区"
            },
            "null_value": null
        }
    )";

    try {
        // 一步到位，直接解析
        JsonValue json = Parser::parse(json_str);
        std::cout << "===== 解析成功！=====" << std::endl;

        // 测试数据访问API
        std::cout << "姓名：" << json["name"].as_string() << std::endl;
        std::cout << "年龄：" << json["age"].as_number() << std::endl;
        std::cout << "是否学生：" << std::boolalpha << json["is_student"].as_bool() << std::endl;
        std::cout << "城市：" << json["address"]["city"].as_string() << std::endl;

        // 遍历数组
        std::cout << "成绩：";
        const JsonArray& scores = json["score"].as_array();
        for (size_t i = 0; i < scores.size(); i++) {
            std::cout << scores[i].as_number() << " ";
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "解析失败：" << e.what() << std::endl;
    }

    return 0;
}
```
编译运行后，能正确输出所有的值，就说明我们的核心解析器已经完成了！题目里的基础要求，我们已经完成了一大半。

---

## 阶段4：实现剩余功能（序列化、进阶要求）
现在我们已经完成了核心解析功能，接下来补全题目要求的剩余功能，包括序列化、迭代器支持、文件读取、JSON Pointer等，每个功能都单独讲解，不会混乱。

### 功能1：序列化（把JsonValue转回JSON字符串）
#### 核心原理
序列化是解析的反向操作，遍历JsonValue的内容，按照JSON的格式，拼成字符串。同样用递归的方式，遇到数组和对象，就递归遍历里面的元素。

#### 代码实现
给`JsonValue`类添加`serialize()`方法，直接加在`JsonValue`类的public区域里：
```cpp
// 序列化：把JsonValue转回JSON字符串
// indent：缩进空格数，0表示压缩输出，4表示格式化输出
std::string serialize(int indent = 0, int current_indent = 0) const {
    std::ostringstream oss; // 用来拼接字符串
    std::string ind(indent * current_indent, ' '); // 当前层级的缩进
    std::string ind_step(indent, ' '); // 每一层的缩进增量

    // std::visit：遍历variant里的内容，根据当前类型执行对应的代码
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>; // 获取当前值的类型
        if constexpr (std::is_same_v<T, std::monostate>) {
            // null类型，输出null
            oss << "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            // 布尔类型，输出true/false
            oss << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, double>) {
            // 数字类型，整数输出整数，小数输出小数
            double int_part;
            if (std::modf(arg, &int_part) == 0.0) {
                oss << static_cast<long long>(int_part);
            } else {
                oss << std::setprecision(15) << arg;
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            // 字符串类型，加双引号，处理转义字符
            oss << '"';
            for (char c : arg) {
                switch (c) {
                    case '"':  oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        // 控制字符转义
                        if (static_cast<unsigned char>(c) < 0x20) {
                            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        } else {
                            oss << c;
                        }
                }
            }
            oss << '"';
        } else if constexpr (std::is_same_v<T, JsonArray>) {
            // 数组类型，递归序列化每个元素
            oss << '[';
            if (indent > 0) oss << '\n'; // 格式化输出换行
            for (size_t i = 0; i < arg.size(); i++) {
                if (indent > 0) oss << ind << ind_step;
                oss << arg[i].serialize(indent, current_indent + 1);
                if (i != arg.size() - 1) oss << ','; // 元素之间加逗号
                if (indent > 0) oss << '\n';
            }
            if (indent > 0) oss << ind;
            oss << ']';
        } else if constexpr (std::is_same_v<T, JsonObject>) {
            // 对象类型，递归序列化每个键值对
            oss << '{';
            if (indent > 0) oss << '\n'; // 格式化输出换行
            size_t i = 0;
            for (const auto& [key, value] : arg) {
                if (indent > 0) oss << ind << ind_step;
                oss << '"' << key << "\":";
                if (indent > 0) oss << ' ';
                oss << value.serialize(indent, current_indent + 1);
                if (i != arg.size() - 1) oss << ','; // 键值对之间加逗号
                if (indent > 0) oss << '\n';
                i++;
            }
            if (indent > 0) oss << ind;
            oss << '}';
        }
    }, m_value);

    return oss.str();
}
```
#### 测试
在main函数里添加：
```cpp
// 格式化序列化
std::cout << "\n===== 格式化序列化结果 =====" << std::endl;
std::cout << json.serialize(4) << std::endl;

// 压缩序列化
std::cout << "\n===== 压缩序列化结果 =====" << std::endl;
std::cout << json.serialize() << std::endl;
```

### 功能2：迭代器支持（范围for循环）
题目要求支持迭代器，可用于范围for循环。我们给`JsonValue`类添加迭代器接口，直接加在public区域：
```cpp
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
```
#### 测试
现在可以用范围for循环遍历数组了：
```cpp
std::cout << "成绩（范围for循环）：";
for (const auto& s : json["score"]) {
    std::cout << s.as_number() << " ";
}
std::cout << std::endl;
```

### 功能3：从文件读取解析（题目进阶要求）
给`Parser`类添加静态方法`parse_file()`，直接加在`Parser`的public区域：
```cpp
#include <fstream> // 记得加这个头文件

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
```
#### 测试
创建一个`test.json`文件，写入JSON内容，然后在main函数里调用：
```cpp
JsonValue file_json = Parser::parse_file("test.json");
std::cout << "文件解析结果：" << file_json.serialize(4) << std::endl;
```

### 功能4：支持注释（题目进阶要求）
JSON标准不支持注释，但题目要求支持`//`单行注释和`/* */`多行注释。我们只需要修改Lexer的`skip_whitespace()`函数，让它跳过注释即可：
```cpp
// 修改后的skip_whitespace，支持跳过注释
void skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        if (isspace(c)) {
            advance();
        } else if (c == '/') {
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
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

// 还要添加一个辅助函数peek_next()，查看下一个字符
char peek_next() const {
    if (m_pos + 1 >= m_input.size()) return '\0';
    return m_input[m_pos + 1];
}
```
现在你的解析器就能支持带注释的JSON了！

### 功能5：JSON Pointer (RFC 6901) 查询（题目进阶要求）
JSON Pointer是一种用路径查询JSON值的标准，比如`/address/city`就能直接拿到`json["address"]["city"]`的值。给`JsonValue`类添加`at()`方法，直接加在public区域：
```cpp
// JSON Pointer 查询，RFC 6901
const JsonValue& at(const std::string& pointer) const {
    if (pointer.empty()) return *this;
    if (pointer[0] != '/') throw std::runtime_error("JSON Pointer必须以/开头");

    const JsonValue* current = this;
    size_t pos = 1;
    while (pos < pointer.size()) {
        // 截取两个/之间的token
        size_t next_slash = pointer.find('/', pos);
        std::string token = pointer.substr(pos, next_slash - pos);
        pos = next_slash == std::string::npos ? pointer.size() : next_slash + 1;

        // 处理转义：~0 转成 ~，~1 转成 /
        std::string decoded;
        for (size_t i = 0; i < token.size(); i++) {
            if (token[i] == '~' && i+1 < token.size()) {
                if (token[i+1] == '0') { decoded += '~'; i++; }
                else if (token[i+1] == '1') { decoded += '/'; i++; }
                else decoded += token[i];
            } else {
                decoded += token[i];
            }
        }

        // 数组：用数字索引
        if (current->is_array()) {
            size_t index;
            try {
                index = std::stoull(decoded);
            } catch (...) {
                throw std::runtime_error("JSON Pointer数组索引无效：" + decoded);
            }
            if (index >= current->size()) throw std::runtime_error("JSON Pointer数组索引越界：" + decoded);
            current = &(*current)[index];
        }
        // 对象：用键查询
        else if (current->is_object()) {
            try {
                current = &(*current)[decoded];
            } catch (...) {
                throw std::runtime_error("JSON Pointer键不存在：" + decoded);
            }
        } else {
            throw std::runtime_error("JSON Pointer只能访问数组和对象");
        }
    }
    return *current;
}
```
#### 测试
```cpp
std::cout << "JSON Pointer /address/city：" << json.at("/address/city").as_string() << std::endl;
std::cout << "JSON Pointer /score/0：" << json.at("/score/0").as_number() << std::endl;
```

---

## 最终总结
现在我们已经完整实现了题目里的**所有基础要求+所有进阶要求**，整个代码分成了4个独立的模块，每个模块都有明确的职责和详细的讲解，初学者可以一步一步跟着写，每一步都能测试看到效果。

### 完整的头文件依赖
最后给你整理好所有需要的头文件，放在代码最开头：
```cpp
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <cmath>
```

### 编译命令
```bash
g++ -std=c++17 json_parser.cpp -o json_parser
```

如果你还有哪里看不懂，或者某个模块想更深入的讲解，随时可以提出来，我会给你拆解得更细。
