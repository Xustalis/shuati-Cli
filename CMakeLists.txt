cmake_minimum_required(VERSION 3.20)

project(shuati-cli VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8 /W4)
    # MSVC filesystem workaround
    add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra)
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

# Source file groups
set(CMD_SOURCES
    src/cmd/main.cpp
    src/cmd/commands.cpp
    src/cmd/services.cpp
    src/cmd/basic_commands.cpp
    src/cmd/manage_commands.cpp
    src/cmd/solve_command.cpp
    src/cmd/list_command.cpp
    src/cmd/view_command.cpp
    src/cmd/test_command.cpp
)

set(CORE_SOURCES
    src/core/problem_manager.cpp
    src/core/mistake_analyzer.cpp
    src/core/sm2_algorithm.cpp
    src/core/judge.cpp
    src/core/compiler_doctor.cpp
    src/core/boot_guard.cpp
    src/core/memory_manager.cpp
    src/core/version.cpp
)

set(ADAPTER_SOURCES
    src/adapters/ai_coach.cpp
    src/adapters/leetcode_crawler.cpp
    src/adapters/codeforces_crawler.cpp
    src/adapters/companion_server.cpp
    src/adapters/luogu_crawler.cpp
    src/adapters/lanqiao_crawler.cpp
    src/adapters/matiji_crawler.cpp
)

set(INFRA_SOURCES
    src/infra/database.cpp
    src/infra/logger.cpp
)

set(UTIL_SOURCES
    src/utils/encoding.cpp
)

# Main executable
add_executable(shuati
    ${CMD_SOURCES}
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${INFRA_SOURCES}
    ${UTIL_SOURCES}
)

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

if(WIN32)
    target_link_libraries(shuati PRIVATE Psapi Shell32)
endif()

# Copy resources to build directory
add_custom_command(TARGET shuati POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:shuati>/resources
)

# Testing setup
enable_testing()

# Helper function to add tests with common settings
function(add_shuati_test TEST_NAME TEST_SOURCE)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs EXTRA_SOURCES LINK_LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TEST_NAME}
        ${TEST_SOURCE}
        ${ARG_EXTRA_SOURCES}
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        fmt::fmt
        ${ARG_LINK_LIBS}
    )

    if(WIN32)
        target_link_libraries(${TEST_NAME} PRIVATE Psapi)
    endif()

    target_include_directories(${TEST_NAME} PRIVATE include)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Version test
add_shuati_test(test_version
    src/tests/test_version_logic.cpp
    EXTRA_SOURCES src/core/version.cpp
)

# Judge complex test
add_shuati_test(test_judge_complex
    src/tests/test_judge_complex.cpp
    EXTRA_SOURCES src/core/judge.cpp
    LINK_LIBS SQLiteCpp cpr::cpr nlohmann_json::nlohmann_json Threads::Threads
)

# Memory test
add_shuati_test(test_memory
    src/tests/test_memory.cpp
    EXTRA_SOURCES
        src/core/memory_manager.cpp
        src/infra/database.cpp
        src/utils/encoding.cpp
    LINK_LIBS SQLiteCpp nlohmann_json::nlohmann_json
)

# Judge security test
add_shuati_test(test_judge_security
    src/tests/test_judge_security.cpp
    EXTRA_SOURCES src/core/judge.cpp
    LINK_LIBS Threads::Threads
)

# Crawler robustness test
add_shuati_test(test_crawler_matiji
    src/tests/test_crawler_matiji.cpp
    EXTRA_SOURCES
        src/adapters/matiji_crawler.cpp
        src/infra/database.cpp
        src/utils/encoding.cpp
    LINK_LIBS SQLiteCpp cpr::cpr nlohmann_json::nlohmann_json
)

# Print configuration summary
message(STATUS "")
message(STATUS "Shuati CLI Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests: Enabled")
message(STATUS "")
