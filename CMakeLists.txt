cmake_minimum_required(VERSION 3.20)

project(shuati-cli VERSION 0.1.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8 /W4 /FS)
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
include_directories(src/cmd)
include_directories(src/cli)
include_directories(src/router)

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
    src/cli/legacy_repl.cpp
    src/router/app_router.cpp
)

set(CORE_SOURCES
    src/core/problem_manager.cpp
    src/core/mistake_analyzer.cpp
    src/core/sm2_algorithm.cpp
    src/core/judge.cpp
    src/core/compiler_doctor.cpp
    src/core/sandbox/sandbox_windows.cpp
    src/core/sandbox/sandbox_linux.cpp
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
)

set(INFRA_SOURCES
    src/infra/database.cpp
    src/infra/logger.cpp
    src/infra/http_client.cpp
)

set(UTIL_SOURCES
    src/utils/encoding.cpp
    src/utils/project_utils.cpp
)

set(TUI_SOURCES
    src/tui/tui_app.cpp
    src/tui/tui_views.cpp
    src/tui/command_specs.cpp
    src/tui/tui_command_catalog.cpp
    src/tui/tui_command_engine.cpp
    src/tui/store/app_state.cpp
)

# Main executable
add_executable(shuati
    ${CMD_SOURCES}
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${INFRA_SOURCES}
    ${UTIL_SOURCES}
    ${TUI_SOURCES}
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

target_compile_definitions(shuati PRIVATE
    SHUATI_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    target_link_libraries(shuati PRIVATE Psapi Shell32)
endif()

# ── Install rules ──────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS shuati RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY resources/ DESTINATION ${CMAKE_INSTALL_DATADIR}/shuati-cli/resources)

# ── CPack DEB configuration ───────────────────────────
set(CPACK_PACKAGE_NAME "shuati-cli")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Shuati CLI - Terminal-based algorithm practice tool")
set(CPACK_PACKAGE_CONTACT "Xustalis <admin@example.com>")

set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Xustalis <admin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.31), libgcc-s1, libstdc++6, libcurl4")
set(CPACK_DEBIAN_PACKAGE_SECTION "education")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
include(CPack)

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
    EXTRA_SOURCES 
        src/core/judge.cpp 
        src/core/sandbox/sandbox_windows.cpp 
        src/core/sandbox/sandbox_linux.cpp
        src/utils/encoding.cpp
    LINK_LIBS SQLiteCpp cpr::cpr nlohmann_json::nlohmann_json Threads::Threads
)

# SM2 auto quality test
add_shuati_test(test_sm2_auto_quality
    src/tests/test_sm2_auto_quality.cpp
    EXTRA_SOURCES src/core/sm2_algorithm.cpp
)

set(CMD_SOURCES_NO_MAIN
    src/cmd/commands.cpp
    src/cmd/services.cpp
    src/cmd/basic_commands.cpp
    src/cmd/manage_commands.cpp
    src/cmd/solve_command.cpp
    src/cmd/list_command.cpp
    src/cmd/view_command.cpp
    src/cmd/test_command.cpp
    src/cli/legacy_repl.cpp
    src/router/app_router.cpp
)

# TUI Tests
add_shuati_test(test_tui_render src/tests/test_tui_render.cpp 
    EXTRA_SOURCES ${TUI_SOURCES} ${UTIL_SOURCES} ${CMD_SOURCES_NO_MAIN} ${CORE_SOURCES} ${ADAPTER_SOURCES} ${INFRA_SOURCES}
    LINK_LIBS ftxui::screen ftxui::dom ftxui::component nlohmann_json::nlohmann_json CLI11::CLI11 cpr::cpr SQLiteCpp replxx::replxx httplib::httplib Threads::Threads
)

add_shuati_test(test_tui_cli_parity
    src/tests/test_tui_cli_parity.cpp
    EXTRA_SOURCES
        src/tui/tui_command_catalog.cpp
)

add_shuati_test(test_tui_performance
    src/tests/test_tui_performance.cpp
    EXTRA_SOURCES
        src/tui/tui_views.cpp
        src/tui/tui_command_catalog.cpp
    LINK_LIBS ftxui::screen ftxui::dom ftxui::component
)

add_shuati_test(test_tui_menu_flow
    src/tests/test_tui_menu_flow.cpp
)

add_shuati_test(test_tui_command_specs
    src/tests/test_tui_command_specs.cpp
    EXTRA_SOURCES
        src/tui/command_specs.cpp
)

# Judge security test
add_shuati_test(test_judge_security
    src/tests/test_judge_security.cpp
    EXTRA_SOURCES
        src/core/judge.cpp
        src/core/sandbox/sandbox_windows.cpp
        src/core/sandbox/sandbox_linux.cpp
        src/utils/encoding.cpp
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

# Crawler unit tests
add_executable(crawler_tests
    src/tests/test_crawlers.cpp
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${INFRA_SOURCES}
    ${UTIL_SOURCES}
)

target_include_directories(crawler_tests PRIVATE
    include
    src/cmd
    src/cli
    src/router
)

target_link_libraries(crawler_tests PRIVATE
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
    target_link_libraries(crawler_tests PRIVATE Psapi Shell32)
endif()

add_test(NAME test_crawlers COMMAND $<TARGET_FILE:crawler_tests>)
