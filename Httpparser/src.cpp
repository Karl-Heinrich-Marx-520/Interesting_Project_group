#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

// 解析状态枚举
enum class ParseState {
    kExpectRequestLine,  // 等待解析请求行
    kExpectHeaders,      // 等待解析请求头
    kExpectBody,         // 等待解析请求体
    kParseComplete,      // 单个请求解析完成
    kParseError          // 报文格式错误
};

// HTTP 请求结果结构体
struct HttpRequest {
    std::string method;     // 请求方法：GET/POST/...
    std::string uri;        // 请求URI：/index.html
    std::string version;    // HTTP版本：HTTP/1.0 / HTTP/1.1
    std::unordered_map<std::string, std::string> headers;  // 请求头键值对
    std::string body;       // 请求体

    // 重置请求对象，用于解析下一个请求时复用
    void Reset() {
        method.clear();
        uri.clear();
        version.clear();
        headers.clear();
        body.clear();
    }
};

class HttpParser {
public:
    HttpParser();
    ~HttpParser() = default;

    // 1. 向缓冲区追加数据（socket 读到数据后调用）
    void Append(const char* data, size_t len);

    // 2. 执行解析主逻辑，返回本次解析出的完整请求数量
    //    返回 0 表示数据不足（拆包），需要等待更多数据
    size_t Parse(std::vector<HttpRequest>& out_requests);

    // 3. 重置解析器（连接断开/出错时调用）
    void Reset();

private:
    // ---------- 内部解析辅助函数 ----------
    // 解析请求行
    ParseState ParseRequestLine(const char* begin, const char* end);
    // 解析单行请求头
    ParseState ParseHeaderLine(const char* begin, const char* end);
    // 解析请求体
    ParseState ParseBody(const char* begin, const char* end);
    // 在字节流中查找 \r\n 分隔符，返回指向 \r 的指针，没找到返回 end
    const char* FindCRLF(const char* begin, const char* end) const;
    // 缓冲区紧缩：将未解析数据移到缓冲区开头，避免内存无限增长
    void ShrinkBuffer();

private:
    std::vector<char> m_buffer;       // 应用层读缓冲区，承接 TCP 字节流
    size_t m_parsed_pos;              // 已解析字节偏移量（标记已处理/未处理边界）
    ParseState m_state;               // 状态机当前状态
    size_t m_content_length;          // 暂存请求体长度（从Content-Length解析而来）
    HttpRequest m_current_request;    // 当前正在解析的请求对象
};

//==================
HttpParser::HttpParser()
    : m_parsed_pos(0)
    , m_state(ParseState::kExpectRequestLine)
    , m_content_length(0) {
}

// 追加数据：直接写入缓冲区末尾
void HttpParser::Append(const char* data, size_t len) {
    m_buffer.insert(m_buffer.end(), data, data + len);
}

// 重置解析器全部状态
void HttpParser::Reset() {
    m_buffer.clear();
    m_parsed_pos = 0;
    m_state = ParseState::kExpectRequestLine;
    m_content_length = 0;
    m_current_request.Reset();
}

// 查找 \r\n 分隔符
const char* HttpParser::FindCRLF(const char* begin, const char* end) const {
    for (const char* p = begin; p != end; ++p) {
        if (*p == '\r' && (p + 1) != end && *(p + 1) == '\n') {
            return p;
        }
    }
    return end;
}

// 缓冲区紧缩：把未解析的数据搬到缓冲区开头，释放已解析的空间
void HttpParser::ShrinkBuffer() {
    if (m_parsed_pos == 0) return;
    size_t unparsed_size = m_buffer.size() - m_parsed_pos;
    // 移动未解析数据到缓冲区头部
    std::copy(m_buffer.begin() + m_parsed_pos, m_buffer.end(), m_buffer.begin());
    m_buffer.resize(unparsed_size);
    m_parsed_pos = 0;
}

// ===================请求行解析=======================
ParseState HttpParser::ParseRequestLine(const char* begin, const char* end) {
    const char* crlf = FindCRLF(begin, end);
    if (crlf == end) {
        // 没找到行分隔符，数据不完整（拆包），保持当前状态等待更多数据
        return ParseState::kExpectRequestLine;
    }

    // 按空格拆分请求行三要素：METHOD  URI  HTTP/VERSION
    const char* space1 = std::find(begin, crlf, ' ');
    if (space1 == crlf) return ParseState::kParseError;
    m_current_request.method.assign(begin, space1);

    const char* space2 = std::find(space1 + 1, crlf, ' ');
    if (space2 == crlf) return ParseState::kParseError;
    m_current_request.uri.assign(space1 + 1, space2);

    m_current_request.version.assign(space2 + 1, crlf);
    if (m_current_request.version != "HTTP/1.1" 
        || m_current_request.version != "HTTP/1.0") {
        return ParseState::kParseError;
    }

    // 解析完成，更新已解析偏移量（跳过 \r\n 两个字节）
    m_parsed_pos += (crlf - begin) + 2;
    // 切换到请求头解析阶段
    return ParseState::kExpectHeaders;
}

// =====================请求头解析==========================
ParseState HttpParser::ParseHeaderLine(const char* begin, const char* end) {
    const char* crlf = FindCRLF(begin, end);
    if (crlf == end) {
        // 行不完整，拆包等待
        return ParseState::kExpectHeaders;
    }

    // 遇到空行（begin == crlf），说明请求头结束
    if (begin == crlf) {
        m_parsed_pos += 2;  // 跳过空行的 \r\n

        // 从头部中提取 Content-Length
        auto it = m_current_request.headers.find("Content-Length");
        if (it != m_current_request.headers.end()) {
            try {
                m_content_length = std::stoul(it->second);
            } catch (...) {
                return ParseState::kParseError;
            }
            // 存在请求体，切换到请求体解析阶段
            return ParseState::kExpectBody;
        } else {
            // 无 Content-Length，认为无请求体（GET/HEAD 等场景），解析完成
            m_content_length = 0;
            return ParseState::kParseComplete;
        }
    }

    // 解析单条头部：Key: Value
    const char* colon = std::find(begin, crlf, ':');
    if (colon == crlf) return ParseState::kParseError;

    std::string key(begin, colon);
    // 兼容冒号后的空格（标准为1个空格）
    const char* value_start = colon + 1;
    while (value_start != crlf && *value_start == ' ') ++value_start;
    std::string value(value_start, crlf);

    m_current_request.headers[key] = value;
    // 更新已解析偏移量
    m_parsed_pos += (crlf - begin) + 2;

    // 继续保持头部解析状态，下一轮解析下一行
    return ParseState::kExpectHeaders;
}

// ===================请求体解析==========================
ParseState HttpParser::ParseBody(const char* begin, const char* end) {
    size_t available = end - begin;
    if (available < m_content_length) {
        // 请求体数据不足（拆包），等待更多数据
        return ParseState::kExpectBody;
    }

    // 精确截取 Content-Length 长度的字节作为请求体
    m_current_request.body.assign(begin, begin + m_content_length);
    m_parsed_pos += m_content_length;

    // 请求体读取完成，整个请求解析完毕
    return ParseState::kParseComplete;
}

//==================粘包 + 拆包统一处理==============
size_t HttpParser::Parse(std::vector<HttpRequest>& out_requests) {
    // 先紧缩缓冲区，复用已解析空间
    ShrinkBuffer();
    size_t parsed_count = 0;

    while (true) {
        const char* begin = m_buffer.data() + m_parsed_pos;
        const char* end = m_buffer.data() + m_buffer.size();
        ParseState next_state = m_state;

        // 根据当前状态分发到对应解析函数
        switch (m_state) {
            case ParseState::kExpectRequestLine:
                next_state = ParseRequestLine(begin, end);
                break;
            case ParseState::kExpectHeaders:
                next_state = ParseHeaderLine(begin, end);
                break;
            case ParseState::kExpectBody:
                next_state = ParseBody(begin, end);
                break;
            case ParseState::kParseError:
                return parsed_count; // 出错直接返回
            default:
                break;
        }

        // 情况1：数据不足（拆包），退出循环等待下一次Append
        if (next_state == ParseState::kExpectRequestLine ||
            next_state == ParseState::kExpectHeaders ||
            next_state == ParseState::kExpectBody) {
            m_state = next_state;
            return parsed_count;
        }

        // 情况2：单个请求解析完成
        if (next_state == ParseState::kParseComplete) {
            // 将完整请求存入结果集
            out_requests.push_back(std::move(m_current_request));
            parsed_count++;

            // 重置状态机，准备解析下一个请求（粘包处理核心）
            m_current_request.Reset();
            m_content_length = 0;
            m_state = ParseState::kExpectRequestLine;
            // 继续循环，用缓冲区剩余数据解析下一个请求
        } else {
            m_state = next_state;
            break;
        }
    }
    return parsed_count;
}
