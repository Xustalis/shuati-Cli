# CI/CD 工作流说明

## 工作流一览

| 文件 | 触发条件 | 功能 |
|------|---------|------|
| `ci.yml` | push/PR to `main` | 代码质量检查 + 三平台构建 + 测试 |
| `release.yml` | `v*` 标签推送 | 构建 + 打包 + 创建 GitHub Release |
| `prepare-release.yml` | 手动触发 | 自动版本号更新 + 打标签 + 触发 release |
| `rollback-release.yml` | 手动触发 | 删除 Release 和标签 |
| `workflow-tests.yml` | push/PR to `main` | actionlint 检查 + 发布工具验证 |

## ci.yml - 持续集成

### 阶段 1: 质量检查 (quality)

运行在 `ubuntu-latest`：

1. **clang-format** - 代码格式检查 (non-blocking)
2. **cppcheck** - 静态分析 (non-blocking)
3. **check_version.py** - 确保 CMakeLists.txt / vcpkg.json / README.md 版本一致

### 阶段 2: 构建矩阵 (build)

三平台并行构建，依赖 quality 通过：

| 平台 | 生成器 | 产物 |
|------|--------|------|
| Windows | Visual Studio 17 2022 | `shuati.exe` + DLLs |
| Linux | Ninja | `shuati` |
| macOS | Ninja | `shuati` |

每个平台执行：配置 → 构建 → 测试 → 打包 → 上传 Artifact

## release.yml - 发布流程

当 `v*` 标签被推送时自动运行：

1. **版本校验** - 确保标签与 CMakeLists.txt / vcpkg.json 版本匹配
2. **三平台构建** - 与 CI 相同的构建流程
3. **测试 + 冒烟测试** - 运行单元测试并验证二进制可用
4. **打包**:
   - Windows: `.zip` + Inno Setup `.exe` 安装包
   - Linux: `.tar.gz` + `.deb`
   - macOS: `.tar.gz`
5. **校验和** - 为所有产物生成 SHA-256
6. **发布** - 创建 GitHub Release 并上传所有资源

### 手动触发选项

- `publish`: 是否创建 Release (默认 true)
- `dry_run`: 仅构建，不发布 (默认 false)

## prepare-release.yml - 发布准备

手动触发，自动化版本号更新流程：

1. 运行 `tools/release/plan_release.py` 分析提交历史
2. 运行 `tools/release/update_versions.py` 更新版本号
3. 提交版本变更 + 打标签 + 推送
4. 触发 `release.yml`

## 发布新版本

### 自动方式

```bash
# 在 GitHub Actions 页面手动触发 prepare-release 工作流
```

### 手动方式

```bash
# 1. 更新版本号 (CMakeLists.txt, vcpkg.json, README.md)
# 2. 更新 CHANGELOG.md
# 3. 提交并打标签
git add -A && git commit -m "chore(release): v0.1.0"
git tag -a v0.1.0 -m "v0.1.0"
git push origin main --tags
```

## 环境变量

| 变量 | 说明 |
|------|------|
| `CMAKE_BUILD_PARALLEL_LEVEL=4` | 并行构建 |
| `VCPKG_BINARY_SOURCES="clear;x-gha,readwrite"` | vcpkg 使用 GHA 缓存 |

## 故障排查

- **vcpkg 缓存失效**: 清除 Actions 缓存后重跑
- **版本校验失败**: 确保 CMakeLists.txt / vcpkg.json / README badge 版本一致
- **Inno Setup 失败**: 检查 `installer/shuati.iss` 中的路径配置
- **测试失败**: 本地 `ctest --test-dir build -C Release --output-on-failure` 复现
