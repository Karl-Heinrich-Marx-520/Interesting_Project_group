#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <netdb.h>      // getaddrinfo
#include <arpa/inet.h>  // inet_ntop
#include <cstring>      // memset

// RAII 封装 addrinfo 资源
// getaddrinfo 分配的资源必须用 freeaddrinfo 释放
struct AddrInfoDeleter {
    void operator()(addrinfo* ptr) const noexcept {
        if (ptr) freeaddrinfo(ptr);
    }
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

// 结构化返回结果
struct IpAddress {
    std::string ip;
    int family; // AF_INET or AF_INET6
};

class DnsResolver {
public:
    /**
     * @brief 解析域名
     * @param domain 域名 (std::string_view 避免不必要的拷贝)
     * @return 包含所有解析到的 IP 地址的 vector
     * @throws std::system_error 如果解析失败
     */
    static std::vector<IpAddress> resolve(std::string_view domain) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;    // 支持 IPv4 和 IPv6
        hints.ai_socktype = SOCK_STREAM; // 假设是 TCP 流
        hints.ai_flags = AI_CANONNAME;   // 获取规范域名

        addrinfo* result_raw = nullptr;
        
        // 调用底层 C API
        int status = getaddrinfo(std::string(domain).c_str(), nullptr, &hints, &result_raw);

        // 现代 C++ 错误处理：将错误码转换为异常
        if (status != 0) {
            // gai_strerror 获取错误描述
            throw std::system_error(std::error_code(status, std::generic_category()), 
                                    "DNS Resolution failed for: " + std::string(domain));
        }

        // 使用智能指针管理生命周期，离开作用域自动 freeaddrinfo
        AddrInfoPtr result_ptr(result_raw);

        std::vector<IpAddress> addresses;
        char ip_str[INET6_ADDRSTRLEN]; // 足够大以容纳 IPv6

        // 遍历链表
        for (addrinfo* p = result_ptr.get(); p != nullptr; p = p->ai_next) {
            void* addr_struct;
            std::string ip_version;

            if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                addr_struct = &(ipv4->sin_addr);
                ip_version = "IPv4";
            } else { // IPv6
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
                addr_struct = &(ipv6->sin6_addr);
                ip_version = "IPv6";
            }

            // 将二进制地址转换为字符串
            inet_ntop(p->ai_family, addr_struct, ip_str, sizeof(ip_str));
            
            addresses.push_back({std::string(ip_str), p->ai_family});
        }

        return addresses;
    }
};

// --- 使用示例 ---
int main() {
    try {
        std::cout << "正在解析 www.orentec.co.kr ...\n";
        auto ips = DnsResolver::resolve("www.orentec.co.kr");

        std::cout << "解析成功! 找到 " << ips.size() << " 个地址:\n";
        for (const auto& ip : ips) {
            std::cout << " - [" << (ip.family == AF_INET ? "IPv4" : "IPv6") 
                      << "] " << ip.ip << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
