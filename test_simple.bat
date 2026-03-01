@echo off
REM 测试简化后的爬虫
echo.
echo ========================================
echo   测试简化爬虫 (只获取HTML)
echo ========================================
echo.

REM 检查build目录
if not exist "build" (
    echo [1/3] 创建build目录...
    mkdir build
)

cd build

REM 配置CMake
echo [2/3] 配置CMake...
cmake .. -DCMAKE_BUILD_TYPE=Debug > nul 2>&1
if errorlevel 1 (
    echo ❌ CMake配置失败
    exit /b 1
)
echo ✅ CMake配置成功

REM 编译
echo [3/3] 编译项目...
cmake --build . --config Debug > nul 2>&1
if errorlevel 1 (
    echo ❌ 编译失败
    exit /b 1
)
echo ✅ 编译成功

echo.
echo ========================================
echo   编译完成
echo ========================================
echo.
echo 下一步:
echo 1. 运行 python ..\test_simple_crawlers.py 测试爬虫
echo 2. 或手动测试:
echo    .\shuati.exe pull https://leetcode.com/problems/two-sum/
echo.
cd ..
