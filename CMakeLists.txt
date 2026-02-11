cmake_minimum_required(VERSION 3.20)

project(shuati-cli VERSION 1.2.0 LANGUAGES CXX)

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
    src/infra/database.cpp
    src/adapters/ai_coach.cpp
    src/adapters/leetcode_crawler.cpp
    src/adapters/codeforces_crawler.cpp
    src/adapters/companion_server.cpp
    src/adapters/luogu_crawler.cpp
    src/adapters/lanqiao_crawler.cpp
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

if(WIN32)
    target_link_libraries(shuati PRIVATE Psapi)
endif()
