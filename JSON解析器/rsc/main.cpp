#include "JsonException.cpp"
#include <Windows.h>



int main() {
    // 强制控制台使用UTF-8编码，解决中文乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    // 关闭同步，提升cout性能（可选）
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
	

    // 测试JSON字符串
    std::string json_str = R"(
        {
            "age": 20,
            "is_student": true,
            "name": "张三",
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
        std::cout << "城市：" << json["address"]["city"].as_string() << json["address"]["district"].as_string() << std::endl;

        // 遍历数组
        std::cout << "成绩：";
        const JsonArray& scores = json["score"].as_array();
        for (size_t i = 0; i < scores.size(); i++) {
            std::cout << scores[i].as_number() << " ";
        }
        std::cout << std::endl;

        std::cout << "成绩（范围for循环）：";
        for (const auto& s : json["score"]) {
            std::cout << s.as_number() << " ";
        }
        std::cout << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "解析失败：" << e.what() << std::endl;
    }

    return 0;
}
