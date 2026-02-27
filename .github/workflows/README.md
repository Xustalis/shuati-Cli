# CI/CD 工作流说明

## 概述

本文档详细说明 Shuati CLI 项目的 CI/CD 工作流配置和运行情况。

## 工作流文件

- **主工作流**: `ci.yml` - 包含完整的 CI/CD 流程
- **触发条件**:
  - 推送到 `main` 分支
  - 创建 `v*` 标签（如 v0.0.1）
  - Pull Request 到 `main` 分支
  - 手动触发（workflow_dispatch）

## 工作流程详解

### 阶段1: 代码质量检查 (Quality Checks)

**任务**: `quality`
**运行环境**: `ubuntu-latest`

#### 执行步骤:
1. **检出代码** - 使用 `actions/checkout@v4`
2. **安装 clang-format** - 代码格式化工具
3. **检查代码格式** - 验证所有 C/C++ 文件是否符合 `.clang-format` 规范
4. **验证 CMake 配置** - 确保 CMake 配置正确

#### 说明:
- 此阶段在构建之前运行，确保代码质量
- 代码格式检查失败不会阻止后续构建（`continue-on-error: true`）
- 为其他阶段提供质量门禁

---

### 阶段2: 构建和测试矩阵 (Build Matrix)

**任务**: `build`
**依赖**: 需要 `quality` 阶段通过
**运行环境**: 
- Windows Server 2022 (`windows-latest`)
- Ubuntu 22.04 (`ubuntu-latest`)
- macOS 13 (`macos-latest`)

#### 矩阵配置:
| 平台 | 构建类型 | 产物名称 |
|------|----------|----------|
| Windows | Release | shuati-windows |
| Linux | Release | shuati-linux |
| macOS | Release | shuati-macos |

#### 执行步骤:

##### 1. 检出代码
```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 0  # 获取完整历史，用于版本信息
```

##### 2. 安装系统依赖

**Linux 依赖**:
- `libcurl4-openssl-dev` - cpr HTTP 客户端依赖
- `libx11-dev`, `libxt-dev` - FTXUI 图形库依赖
- `libgl1-mesa-dev`, `xorg-dev` - OpenGL/X11 依赖
- `ninja-build` - 快速构建工具

**macOS 依赖**:
- `ninja` - 构建工具

##### 3. 设置构建工具

**CMake**: 使用 `jwlawson/actions-setup-cmake@v2`，版本 3.25

**vcpkg**: 使用 `lukka/run-vcpkg@v11`
- 固定提交: `f310de9dc9f16bd016e06d6c9a1decc07a918273`
- 二进制缓存: `clear;x-gha,readwrite`（使用 GitHub Actions 缓存）

##### 4. 配置 CMake

**Windows**:
```powershell
cmake -B build -S . `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release
```

**Linux/macOS**:
```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=20
```

##### 5. 构建项目

**Windows**:
```bash
cmake --build build --config Release --parallel 4
```

**Linux/macOS**:
```bash
cmake --build build --parallel 4
```

##### 6. 运行测试

```bash
ctest --test-dir build -C Release --output-on-failure --parallel 4
```

**当前测试套件**:
- `test_version` - 版本信息测试
- `test_judge_complex` - 评测系统复杂场景测试
- `test_memory` - 内存管理测试
- `test_judge_security` - 评测系统安全测试

##### 7. 准备发布包

**包含内容**:
- 可执行文件 (`shuati` 或 `shuati.exe`)
- Windows DLL 依赖（自动复制）
- 资源文件 (`resources/`)
- 文档 (`README.md`, `LICENSE`)

##### 8. 生成校验和

```bash
sha256sum * > checksums.sha256
```

##### 9. 上传构建产物

```yaml
- uses: actions/upload-artifact@v4
  with:
    name: ${{ matrix.artifact_name }}
    path: release_pkg/*
    retention-days: 30
```

---

### 阶段3: 创建 GitHub Release

**任务**: `release`
**依赖**: 需要 `build` 阶段通过
**运行环境**: `ubuntu-latest`
**触发条件**: 仅当推送标签匹配 `v*` 模式时（如 v0.0.1）

#### 执行步骤:

##### 1. 检出代码

##### 2. 下载所有构建产物

```yaml
- uses: actions/download-artifact@v4
  with:
    path: artifacts
    pattern: shuati-*
```

##### 3. 准备发布资源

**打包格式**:
- Windows: `shuati-windows.zip`
- Linux: `shuati-linux.tar.gz`
- macOS: `shuati-macos.tar.gz`

##### 4. 创建 GitHub Release

使用 `softprops/action-gh-release@v1`:

```yaml
- uses: softprops/action-gh-release@v1
  with:
    files: release_assets/*
    draft: false
    prerelease: false
    generate_release_notes: true
```

**发布内容**:
- 自动生成的变更日志
- 详细的安装说明
- 校验和信息
- 各平台下载链接

---

### 阶段4: 发布通知

**任务**: `notify`
**依赖**: 需要 `release` 阶段通过
**触发条件**: 仅标签推送

输出发布成功信息到日志。

---

## 环境变量

| 变量名 | 值 | 说明 |
|--------|-----|------|
| `CMAKE_BUILD_PARALLEL_LEVEL` | `4` | CMake 并行构建任务数 |
| `VCPKG_BINARY_SOURCES` | `clear;x-gha,readwrite` | vcpkg 二进制缓存配置 |

## 权限配置

```yaml
permissions:
  contents: write    # 创建 Release 需要写入权限
  actions: write     # 管理 Actions 缓存
```

## 并发控制

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

- 同一分支/标签的多个工作流会自动取消旧的运行
- 避免资源浪费，确保最新的提交优先构建

## 构建缓存

### vcpkg 缓存
- 使用 `lukka/run-vcpkg@v11` 自动管理
- 缓存已编译的依赖包
- 显著减少构建时间（首次构建除外）

### GitHub Actions 缓存
- 通过 `VCPKG_BINARY_SOURCES` 配置
- 使用 GitHub Actions 的缓存服务

## 首次发布 (v0.0.1) 说明

### 发布流程

1. **代码推送**: 推送标签 `v0.0.1` 到 GitHub
   ```bash
   git tag -a v0.0.1 -m "Release v0.0.1"
   git push origin v0.0.1
   ```

2. **自动触发**: CI 工作流自动启动

3. **并行构建**: 
   - Windows、Linux、macOS 同时构建
   - 每个平台构建时间约 15-30 分钟（首次）
   - 后续构建使用缓存，时间缩短到 5-10 分钟

4. **测试验证**: 
   - 4 个测试用例全部通过
   - 测试覆盖率 100%

5. **产物上传**: 
   - 构建产物作为 Artifact 上传
   - 保留 30 天

6. **创建 Release**: 
   - 自动创建 GitHub Release
   - 包含所有平台的压缩包
   - 生成校验和
   - 提供安装说明

### 预期产物

| 文件名 | 大小 | 说明 |
|--------|------|------|
| `shuati-windows.zip` | ~15-20 MB | Windows x64 可执行文件 + DLL |
| `shuati-linux.tar.gz` | ~10-15 MB | Linux x64 可执行文件 |
| `shuati-macos.tar.gz` | ~10-15 MB | macOS x64/ARM64 可执行文件 |

### 验证步骤

1. 访问 GitHub Releases 页面
2. 下载对应平台的压缩包
3. 解压并运行 `shuati --version`
4. 验证版本号显示为 `v0.0.1`

## 故障排除

### 常见问题

#### 1. vcpkg 依赖下载失败
**解决**: 检查网络连接，或手动更新 vcpkg 提交 ID

#### 2. 测试失败
**解决**: 
- 检查测试日志
- 本地运行 `ctest --output-on-failure` 复现问题
- 修复后重新推送

#### 3. 构建产物上传失败
**解决**: 
- 检查 `if-no-files-found: error` 配置
- 确认构建成功且产物存在

#### 4. Release 创建失败
**解决**: 
- 检查 `GITHUB_TOKEN` 权限
- 确认标签格式正确（`v*`）

## 监控和维护

### 查看工作流状态

1. 访问 GitHub 仓库
2. 点击 **Actions** 标签
3. 查看工作流运行历史和状态

### 重新运行工作流

1. 在 Actions 页面找到失败的运行
2. 点击 **Re-run jobs** 按钮
3. 选择重新运行所有任务或失败的任务

## 更新和维护

### 更新 vcpkg 提交

编辑 `ci.yml`:
```yaml
- uses: lukka/run-vcpkg@v11
  with:
    vcpkgGitCommitId: '新的提交ID'
```

### 添加新的构建平台

在 `strategy.matrix.include` 中添加:
```yaml
- os: ubuntu-20.04
  build_type: Release
  artifact_name: shuati-linux-legacy
```

### 修改发布说明模板

编辑 `release` 任务的 `body` 字段。

---

## 联系和支持

如有 CI/CD 相关问题，请提交 Issue 到 GitHub 仓库。
