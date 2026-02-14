cmake_minimum_required(VERSION 3.20)

project(shuati-cli VERSION 1.5.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Prevent min/max macros on Windows
if(WIN32)
    add_definitions(-DNOMINMAX)
endif()

# Find packages provided by vcpkg
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(replxx CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Add source files
add_executable(shuati
    src/cmd/main.cpp
    src/core/problem_manager.cpp
    src/core/mistake_analyzer.cpp
    src/core/sm2_algorithm.cpp
    src/core/judge.cpp
    src/core/compiler_doctor.cpp
    src/core/boot_guard.cpp
    src/core/memory_manager.cpp
    src/infra/database.cpp
    src/adapters/ai_coach.cpp
    src/adapters/leetcode_crawler.cpp
    src/adapters/codeforces_crawler.cpp
    src/adapters/companion_server.cpp
    src/adapters/luogu_crawler.cpp
    src/adapters/lanqiao_crawler.cpp
    src/core/version.cpp
    src/infra/logger.cpp
    src/utils/encoding.cpp
)

# Link libraries
target_link_libraries(shuati PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    cpr::cpr
    SQLiteCpp
    fmt::fmt
    replxx::replxx
    ftxui::screen
    ftxui::dom
    ftxui::component
    httplib::httplib
    Threads::Threads
)

target_compile_definitions(shuati PRIVATE SHUATI_VERSION="${PROJECT_VERSION}")

# Copy resources to build directory
add_custom_command(TARGET shuati POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:shuati>/resources
)


if(WIN32)
    target_link_libraries(shuati PRIVATE Psapi Shell32)
endif()

# Test executable
add_executable(test_version
    src/tests/test_version_logic.cpp
    src/core/version.cpp
)
target_link_libraries(test_version PRIVATE fmt::fmt)
target_include_directories(test_version PRIVATE include)

# Complex Judge Test
add_executable(test_judge_complex
    src/tests/test_judge_complex.cpp
    src/core/judge.cpp
)
target_link_libraries(test_judge_complex PRIVATE 
    fmt::fmt 
    SQLiteCpp 
    cpr::cpr 
    nlohmann_json::nlohmann_json
    Threads::Threads
)
if(WIN32)
    target_link_libraries(test_judge_complex PRIVATE Psapi)
endif()
target_include_directories(test_judge_complex PRIVATE include)

# Memory Test
add_executable(test_memory
    src/tests/test_memory.cpp
    src/core/memory_manager.cpp
    src/infra/database.cpp
    src/utils/encoding.cpp
)
target_link_libraries(test_memory PRIVATE 
    fmt::fmt 
    SQLiteCpp 
    nlohmann_json::nlohmann_json
)
target_include_directories(test_memory PRIVATE include)

# Security Test
add_executable(test_judge_security
    src/tests/test_judge_security.cpp
    src/core/judge.cpp
)
target_link_libraries(test_judge_security PRIVATE 
    fmt::fmt 
    Threads::Threads
)
if(WIN32)
    target_link_libraries(test_judge_security PRIVATE Psapi)
endif()
target_include_directories(test_judge_security PRIVATE include)
