# 核心结论：**传右值给 View 视图 = 直接悬空，完全失效，必炸野指针**
`std::string_view` / `std::span` **只是裸指针+长度**，**不持有数据、不延长生命周期**。
右值是**临时对象**，表达式结束立马销毁，视图还指着已经被销毁的内存，直接悬空引用、未定义行为。

## 一、为什么右值传 View 不行？
1. 右值（临时 `std::string`、临时 `std::vector<Token>`）生命周期：**整行表达式结束立刻析构销毁**。
2. `string_view`/`span` 只存起始地址和长度，**不做拷贝、不接管内存、不续命**。
3. 临时对象销毁后，视图指向**已释放的堆内存**，后续读数据直接崩溃、乱码。

### 危险示例（绝对不能写）
```cpp
// 传 std::string 临时右值
Lexer lex(std::string("{\"name\":\"test\"}")); 
// 临时string构造完 → 构造函数结束 → 临时立刻销毁
// lex.m_input(string_view) 指向一块已销毁内存，后续lex分词直接崩

// 传 vector<Token> 临时右值
Parser parser(std::vector<Token>{...});
// 临时vector立马销毁，span悬空
```

**字符串字面量除外**：
```cpp
Lexer lex(R"({"a":1})"); 
```
字面量是**静态常量区**，生命周期全局，不是栈临时右值，用 `string_view` 完全安全。

## 二、根本问题
View 视图的设计定位：
> **只用来借用「生命周期更长的左值」，绝不借用临时右值**

## 三、工程完美解决方案：重载构造函数
给 Lexer / Parser 做**双层重载**：
1. **左值引用** → 用 View 零拷贝借用（高性能）
2. **右值引用** → 用**移动语义**接管所有权（安全，无深拷贝）

### 1. Lexer 改造（兼容左值+右值）
```cpp
class Lexer {
private:
    std::string_view m_view;
    std::string m_own_str; // 仅右值时持有数据
    size_t m_pos{}, m_line{}, m_col{};

public:
    // 左值：用view，零拷贝
    explicit Lexer(const std::string& input) 
        : m_view(input) {}

    // 右值：移动接管，不深拷贝，延长生命周期
    explicit Lexer(std::string&& input) 
        : m_own_str(std::move(input)), m_view(m_own_str) {}

    // 字面量/常量字符串 直接匹配string_view
    explicit Lexer(std::string_view input) 
        : m_view(input) {}
};
```

### 2. Parser 改造（兼容左值+右值）
```cpp
class Parser {
private:
    std::span<const Token> m_span;
    std::vector<Token> m_own_tokens; // 仅右值时持有

public:
    // 左值：span视图借用
    explicit Parser(const std::vector<Token>& tokens)
        : m_span(tokens) {}

    // 右值：移动接管
    explicit Parser(std::vector<Token>&& tokens)
        : m_own_tokens(std::move(tokens)), m_span(m_own_tokens) {}
};
```

## 四、这样写的好处
1. **传左值**：走 View 视图，**零拷贝、高性能**；
2. **传右值临时**：走移动语义，**仅转移堆指针，无内存拷贝**，且内部成员持有数据，生命周期和对象一致，**绝对安全**；
3. 外部调用完全不用改代码，自动匹配重载，无感优化。

## 五、极简总结
- **普通临时右值** → 不能用 View，必悬空；
- **字符串字面量/全局常量** → 可以用 `string_view` 安全使用；
- **最佳实践**：重载构造，**左值视图借用，右值移动接管**，兼顾性能与安全，彻底规避深拷贝 + 悬空问题。
