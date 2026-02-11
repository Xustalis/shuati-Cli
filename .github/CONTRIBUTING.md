# 贡献指南 (Contributing Guide)

感谢你对 **Shuati CLI** 的关注！我们欢迎任何形式的贡献，包括提交 Bug、改进文档、提出新功能建议或直接提交代码。

## 🛠️ 开发环境搭建

### 依赖项
- **CMake** 3.20+
- **C++20** 编译器 (MSVC v143+, GCC 10+, Clang 12+)
- **vcpkg** (用于依赖管理)

### 依赖库 (通过 vcpkg 自动安装)
- CLI11
- nlohmann/json
- cpr
- fmt
- replxx
- sqlite3
- ftxui

### ⚡ 快速上手

1. **Fork 本仓库** 并 Clone 到本地：
   ```bash
   git clone https://github.com/YOUR_USERNAME/shuati-cli.git
   cd shuati-cli
   ```

2. **配置 CMake** (假设 vcpkg 已安装并配置环境变量，或使用 vcpkg 子模块)：
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
   ```

3. **编译项目**：
   ```bash
   cmake --build build --config Release
   ```

4. **运行测试**：
   ```bash
   cd build
   ctest -C Release
   ```

## 🐛 提交 Issue
在提交 Issue 之前，请确保：
1. 搜索现有的 Issues，避免重复。
2. 使用提供的 Issue 模板。
3. 清晰描述 Bug 的复现步骤或新功能的具体需求。

## 🔄 提交 Pull Request (PR)
1. 从 `main` 分支切出一个新分支 (`feature/amazing-feature` 或 `fix/bug-fix`)。
2. 提交代码，请确保代码风格一致，并通过了编译。
3. 提交 PR，并在描述中关联相关的 Issue (例如 `Closes #123`)。
4. 等待 Maintainer Review。

## 📜 代码规范
- 使用 C++20 标准。
- 遵循 Google C++ Style Guide (或项目现有的 `.clang-format`)。
- 变量命名使用 `snake_case`，类名使用 `PascalCase`。

感谢你的贡献！🚀
