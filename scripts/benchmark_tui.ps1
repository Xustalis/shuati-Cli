param(
    [string]$BuildDir = "build",
    [string]$OutputFile = "tui-benchmark-report.txt"
)

$ErrorActionPreference = "Stop"

cmake --build $BuildDir --config Release --target test_tui_performance
$exePath = Join-Path $BuildDir "Release\\test_tui_performance.exe"
$perfResult = & $exePath
$ctestResult = ctest --test-dir $BuildDir -C Release --output-on-failure -R "test_tui_performance"

$content = @(
    "=== TUI Performance Metrics ==="
    $perfResult
    ""
    "=== Test Result ==="
    $ctestResult
)
$content | Set-Content -Path $OutputFile -Encoding UTF8
$content
