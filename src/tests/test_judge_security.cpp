#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include "shuati/judge.hpp"
#include "shuati/utils/encoding.hpp"

using namespace shuati;

namespace {

void remove_if_exists(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void test_cleanup_with_unicode_path() {
    Judge judge;

    auto temp_dir = std::filesystem::temp_directory_path();
    auto source_path = temp_dir / shuati::utils::utf8_path("shuati_三元组中心问题_cleanup.cpp");

#ifdef _WIN32
    auto expected_exe_path = temp_dir / shuati::utils::utf8_path("shuati_三元组中心问题_cleanup.exe");
#else
    auto expected_exe_path = temp_dir / shuati::utils::utf8_path("shuati_三元组中心问题_cleanup");
#endif

    remove_if_exists(source_path);
    remove_if_exists(expected_exe_path);

    {
        std::ofstream f(source_path);
        f << "int main() { return 0; }";
    }

    std::string executable;
    try {
        executable = judge.prepare(shuati::utils::path_to_utf8(source_path), "cpp");
    } catch (const std::exception& e) {
        remove_if_exists(source_path);
        std::cerr << "Failed: Unicode path compile should succeed, but got: " << e.what() << "\n";
        exit(1);
    }

    auto executable_path = shuati::utils::utf8_path(executable);
    if (!std::filesystem::exists(executable_path)) {
        remove_if_exists(source_path);
        std::cerr << "Failed: Compiled executable not found for Unicode path\n";
        exit(1);
    }

    try {
        judge.cleanup_prepared(executable, "cpp");
    } catch (const std::exception& e) {
        remove_if_exists(source_path);
        remove_if_exists(executable_path);
        std::cerr << "Failed: Unicode executable cleanup threw exception: " << e.what() << "\n";
        exit(1);
    }

    if (std::filesystem::exists(executable_path)) {
        remove_if_exists(source_path);
        remove_if_exists(executable_path);
        std::cerr << "Failed: Unicode executable cleanup did not remove the file\n";
        exit(1);
    }

    remove_if_exists(source_path);
    std::cout << "Unicode cleanup path test passed!\n";
}

void test_cleanup_with_ascii_path() {
    Judge judge;

    auto temp_dir = std::filesystem::temp_directory_path();
    auto source_path = temp_dir / "shuati_ascii_cleanup.cpp";

#ifdef _WIN32
    auto expected_exe_path = temp_dir / "shuati_ascii_cleanup.exe";
#else
    auto expected_exe_path = temp_dir / "shuati_ascii_cleanup";
#endif

    remove_if_exists(source_path);
    remove_if_exists(expected_exe_path);

    {
        std::ofstream f(source_path);
        f << "int main() { return 0; }";
    }

    std::string executable;
    try {
        executable = judge.prepare(source_path.string(), "cpp");
    } catch (const std::exception& e) {
        remove_if_exists(source_path);
        std::cerr << "Failed: ASCII path compile should succeed, but got: " << e.what() << "\n";
        exit(1);
    }

    auto executable_path = shuati::utils::utf8_path(executable);
    if (!std::filesystem::exists(executable_path)) {
        remove_if_exists(source_path);
        std::cerr << "Failed: Compiled executable not found for ASCII path\n";
        exit(1);
    }

    judge.cleanup_prepared(executable, "cpp");

    if (std::filesystem::exists(executable_path)) {
        remove_if_exists(source_path);
        remove_if_exists(executable_path);
        std::cerr << "Failed: ASCII executable cleanup did not remove the file\n";
        exit(1);
    }

    remove_if_exists(source_path);
    std::cout << "ASCII cleanup path test passed!\n";
}

} // namespace

void test_security() {
    Judge judge;
    
    // 1. Test invalid characters in filename
    std::string malicious_path = "test\"; echo pwned; \".cpp";
    try {
        judge.prepare(malicious_path, "cpp");
        std::cerr << "Failed: Should have thrown exception for malicious path\n";
        exit(1);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (msg.find("Invalid source file path") == std::string::npos) {
            std::cerr << "Failed: Unexpected exception message: " << msg << "\n";
            exit(1);
        }
        std::cout << "Caught expected security exception: " << msg << "\n";
    }

    // 2. Test valid path
    std::string safe_path = "safe_test.cpp";
    // Create a dummy file
    {
        std::ofstream f(safe_path);
        f << "int main() { return 0; }";
    }
    
    try {
        // This might fail if no compiler, but shouldn't throw security error
        // We just want to ensure it passes the security check
        // The compile function might throw "Compilation failed" or similar, 
        // but checking the *path* happens first.
        judge.prepare(safe_path, "cpp");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (msg.find("Invalid source file path") != std::string::npos) {
             std::cerr << "Failed: Safe path rejected: " << msg << "\n";
             exit(1);
        }
        // Other errors (compilation failed) are fine for this test
    }
    
    try {
        std::filesystem::remove(safe_path);
    } catch(...) {}

    std::cout << "test_judge_security passed!\n";
}

int main() {
    test_security();
    test_cleanup_with_unicode_path();
    test_cleanup_with_ascii_path();
    return 0;
}
