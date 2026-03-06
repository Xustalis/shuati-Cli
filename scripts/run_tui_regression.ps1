param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

cmake --build $BuildDir --config Release
ctest --test-dir $BuildDir -C Release --output-on-failure -R "test_tui_render|test_tui_cli_parity|test_tui_performance|test_tui_menu_flow"
