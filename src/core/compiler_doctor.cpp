#include "shuati/compiler_doctor.hpp"
#include <regex>
#include <fmt/core.h>

namespace shuati {

Diagnosis CompilerDoctor::diagnose(const std::string& error_output) {
    Diagnosis d;
    d.title = "未知错误";
    d.description = "无法识别的编译错误，请检查代码逻辑。";
    d.suggestion = "请尝试阅读上方的英文报错信息，或者使用 'hint' 命令询问 AI。";

    // 0. Unsupported C++ standard flag
    std::smatch std_m;
    if (std::regex_search(error_output, std_m, std::regex(R"(unrecognized\s+command\s+line\s+option\s+'(-std=[^']+)')"))) {
        d.title = "编译器过旧：不支持该标准";
        d.description = fmt::format("当前 g++ 不支持参数 {}。", std_m[1].str());
        d.suggestion = "建议升级 MinGW-w64 / GCC 版本，或切换到支持 C++20 的编译器。也可临时改用较低标准（如 C++17）。";
        return d;
    }

    // 1. Missing semicolon
    // error: expected initializer before '...'
    // error: expected ';' before '...'
    if (std::regex_search(error_output, std::regex(R"(expected\s*['"]?[:;]['"]?\s*before)"))) {
        d.title = "语法错误：缺少分号";
        d.description = "代码中某行末尾可能遗漏了分号 ';'";
        d.suggestion = "请检查报错行及其上一行的末尾，确保每条语句都以分号结束。";
        return d;
    }

    // 2. Chinese characters
    // error: stray '\357' in program
    if (std::regex_search(error_output, std::regex(R"(stray\s*['\\]\d+)"))) {
        d.title = "字符错误：非法字符";
        d.description = "代码中检测到非法的 ASCII 字符，通常是使用了中文标点符号。";
        d.suggestion = "请检查分号、括号、引号是否误用了中文输入法。";
        return d;
    }

    // 3. Undeclared variable
    // error: '...' was not declared in this scope
    std::smatch m;
    if (std::regex_search(error_output, m, std::regex(R"('([^']+)'\s+was\s+not\s+declared)"))) {
        d.title = "变量未定义";
        d.description = fmt::format("变量 '{}' 未定义或未声明。", m[1].str());
        d.suggestion = "1. 检查拼写是否正确。\n2. 确保在使用该变量前已定义它。\n3. 检查是否遗漏了头文件或 using namespace。";
        return d;
    }

    // 4. Index out of bounds (Runtime)
    // index ... out of bounds
    if (std::regex_search(error_output, std::regex(R"(index\s+\d+\s+out\s+of\s+bounds)", std::regex::icase))) {
        d.title = "运行时错误：数组越界";
        d.description = "访问了不存在的数组索引。";
        d.suggestion = "请检查循环边界，确保索引在 [0, size-1] 范围内。";
        return d;
    }
    
    // 5. Linker error (missing main or duplicate symbol)
    if (error_output.find("undefined reference to `main'") != std::string::npos ||
        error_output.find("LNK2019") != std::string::npos && error_output.find("main") != std::string::npos) {
        d.title = "链接错误：找不到入口点";
        d.description = "未找到 main 函数。";
        d.suggestion = "请确保代码中包含 'int main() { ... }'。";
        return d;
    }

    return d;
}


bool CompilerDoctor::check_environment(std::vector<std::string>& missing_tools) {
    missing_tools.clear();
    
#ifdef _WIN32
    const char* null_dev = "nul";
#else
    const char* null_dev = "/dev/null";
#endif

    // Check g++
    std::string cmd_cpp = fmt::format("g++ --version > {} 2>&1", null_dev);
    if (std::system(cmd_cpp.c_str()) != 0) {
        missing_tools.push_back("g++ (MinGW/GCC)");
    }
    
    // Check python
    std::string cmd_py = fmt::format("python --version > {} 2>&1", null_dev);
    if (std::system(cmd_py.c_str()) != 0) {
        // Try python3
        std::string cmd_py3 = fmt::format("python3 --version > {} 2>&1", null_dev);
        if (std::system(cmd_py3.c_str()) != 0) {
            missing_tools.push_back("python");
        }
    }
    
    return missing_tools.empty();
}

} // namespace shuati
