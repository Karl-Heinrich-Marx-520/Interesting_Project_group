# `std::visit + auto模板lambda + if constexpr`
# 为什么是 C++ 处理 `std::variant` **最标准、最正统、最优雅** 的固定三件套
我拆成 **3个角色、执行流程、缺一不可的原因、对比老式写法**，结合你 JSON 源码逐行讲。

---

## 一、先拆：三个组件各自是什么角色
### 1. `std::visit` → **自动类型调度器**
你不用自己写：
```cpp
if (holds_alternative<Null>(v))
else if (holds_alternative<bool>(v))
else if (holds_alternative<double>(v))
...
```
`std::visit` 内部帮你做了：
1. 自动检测 `variant` 当前**真实存的是哪一种类型**
2. 自动把里面的值**转发**给后面的 lambda
3. 自动帮你遍历、分发，不用手动写一长串类型判断

一句话：**帮你省去手动写一堆 `is_xxx` 类型判断的脏活**。

---

### 2. `[&](const auto& arg)` → **泛型模板 Lambda**
普通 lambda 参数不能泛型，但**参数写 `auto` 的 lambda 天生就是模板**。

编译器背地里把你这句：
```cpp
[&](const auto& arg) { ... }
```
自动翻译成等价的模板仿函数：
```cpp
template <typename T>
struct Lambda {
    template <typename Arg>
    void operator()(const Arg& arg) const {
        // 你里面的所有代码
    }
};
```
作用：
**一个 lambda 就能接收 variant 里所有 6 种类型**，不用写6次重载。

---

### 3. `if constexpr` → **编译期分支裁剪器**
这是最关键的一环。
因为 lambda 是**模板**，`T` 每一次都不一样：
- 某次实例化 `T = std::monostate`
- 某次实例化 `T = bool`
- 某次实例化 `T = JsonArray`
- 某次实例化 `T = JsonObject`

如果用**普通 if**：
编译器会把**所有分支代码全部编译检查**。
比如 `T=bool` 时，代码里还有 `serialize_array、serialize_object` 这些调用，bool 根本不支持，**直接编译报错**。

`if constexpr` 作用：
**编译期就判断类型，不匹配的分支直接删掉、不编译、不检查语法**。
每个类型只保留自己那一行逻辑，其余全部丢弃。

---

## 二、整套组合的完整执行流程（跟着你代码走一遍）
你的 variant：
```cpp
variant<std::monostate, bool, double, string, JsonArray, JsonObject>
```

### 流程1：编译器给每种类型**单独生成一份 lambda 实例**
一共生成 **6 个完全独立的函数版本**：
1. 专给 `monostate` 用的版本
2. 专给 `bool` 用的版本
3. 专给 `double` 用的版本
4. 专给 `string` 用的版本
5. 专给 `JsonArray` 用的版本
6. 专给 `JsonObject` 用的版本

👉 **重点：6个版本互不干扰、不是共用同一份代码**

### 流程2：每个版本内部 `if constexpr` 自行裁剪
以 `T = JsonArray` 举例：
```cpp
if constexpr(monostate?) 否 → 删
else if constexpr(bool?)  否 → 删
else if constexpr(double?)否 → 删
else if constexpr(string?)否 → 删
else if constexpr(JsonArray?)是 → 保留 serialize_array
剩下 object 分支 → 删
```
最终这个函数**只剩一行有效代码**。

同理：
`JsonObject` 版本只保留 `serialize_object`，其他全删。

### 流程3：运行时 `std::visit` 自动选对应版本
你 `variant` 存的是什么类型，就调用**对应那份已经裁剪好的专属函数**。

---

## 三、为什么必须三件套缺一不可？
### 少了 `std::visit`
你就得手写巨长一坨：
```cpp
if (std::holds_alternative<...>) {
    auto& val = std::get<...>(v);
    // 处理
} else if (...)
```
啰嗦、冗余、容易漏类型、难维护。

### 少了 `auto 模板lambda`
你必须**手动写一个重载结构体**，每个类型写一个重载函数：
```cpp
struct Visitor {
    void operator()(std::monostate) { ... }
    void operator()(bool) { ... }
    void operator()(double) { ... }
    ...
};
```
代码多一倍，不够简洁。

### 少了 `if constexpr`
换成普通 `if` → **必编译报错**。
因为模板实例化时，所有分支代码都会被语法检查，类型不匹配直接炸。

---

## 四、为什么说这是「最正统标准写法」？
C++17 之后，标准库设计者 **就是为了让你这么用** 设计的：
- `std::variant` 存多类型
- `std::visit` 统一调度
- 泛型 lambda 简化模板写法
- `if constexpr` 解决模板分支编译错误

行业里所有 JSON 库、解析器、状态机、多类型容器，**清一色全是这套写法**。

---

## 五、解答你之前最大的疑惑
> 我先存 object 裁掉其他分支，再存 array 咋办？

完全不冲突：
- 不是**同一份函数共用**
- 是**每种类型单独生成一个专属函数**
- 每个专属函数自己裁剪自己的分支
- Object 走 Object 的函数，Array 走 Array 的函数，互不影响、互不覆盖

你的代码写法 **完全正确、完全标准**，没有任何问题。

---

## 六、一句话终极总结
1. **std::visit**：帮你自动分发类型，不用手写一堆 `if`
2. **auto 模板lambda**：一个函数模板吃下 variant 所有类型
3. **if constexpr**：编译期砍掉不匹配分支，避免编译报错、还零运行开销

三者组合 = **C++17 处理 variant 的工业标准写法**。
