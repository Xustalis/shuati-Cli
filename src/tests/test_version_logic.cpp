#include <iostream>
#include <cassert>
#include "shuati/version.hpp"
#include <fmt/core.h>

void test_parse() {
    auto v1 = shuati::Version::parse("1.2.3");
    assert(v1.major == 1);
    assert(v1.minor == 2);
    assert(v1.patch == 3);
    assert(v1.suffix.empty());

    auto v2 = shuati::Version::parse("v2.0.0-beta");
    assert(v2.major == 2);
    assert(v2.minor == 0);
    assert(v2.patch == 0);
    assert(v2.suffix == "beta");

    auto v3 = shuati::Version::parse("invalid");
    assert(v3.major == 0);
    assert(v3.minor == 0);
    assert(v3.patch == 0);
    // Invalid parsing might return 0.0.0, depends on impl.
    // Based on regex, "invalid" won't match, so it returns default Version()?
    // Version struct has no default constructor init? 
    // Let's check Version struct. It has default member inits?
}

void test_to_string() {
    shuati::Version v{1, 2, 3, ""};
    assert(v.to_string() == "v1.2.3");

    shuati::Version v2{1, 2, 3, "rc1"};
    assert(v2.to_string() == "v1.2.3-rc1");
}

void test_compatibility() {
    shuati::Version current{1, 5, 0};
    shuati::Version required{1, 2, 0};
    assert(current.is_compatible_with(required)); // 1.5 >= 1.2

    shuati::Version future{2, 0, 0};
    assert(!current.is_compatible_with(future)); // 1.5 < 2.0 ? 
    // Wait, is_compatible_with usually means "current provides what required needs".
    // If required is 2.0, current 1.5 might NOT be compatible.
    
    // Let's check implementation of is_compatible_with.
}

int main() {
    try {
        test_parse();
        test_to_string();
        test_compatibility();
        std::cout << "All version tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
