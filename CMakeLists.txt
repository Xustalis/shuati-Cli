cmake_minimum_required(VERSION 3.20)

project(shuati-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(MSVC)
    add_compile_options(/utf-8)
endif()

# Find packages provided by vcpkg
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(replxx CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define executable
add_executable(shuati ${SOURCES})

# Link libraries
target_link_libraries(shuati PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    cpr::cpr
    SQLiteCpp
    fmt::fmt
    replxx::replxx
)
