# Changelog

All notable changes to this project will be documented in this file.

## [0.0.2] - 2026-03-01

### 新增功能 (New Features)
- **爬虫架构全面升级**：重构了全平台爬虫（Codeforces, LeetCode, 洛谷, 蓝桥云课），现已支持提取完整的题目结构化元数据（包含完整描述文本、测试用例、题目标签及难度等）。
- **LeetCode (GraphQL)**：全面废弃正则/HTML匹配方案，采用官方 GraphQL API 获取精准详细完整的题目内容，全面兼容 `leetcode.com` 与 `leetcode.cn`。
- **洛谷 (Luogu)**：深度适配洛谷最新的 `lentille-context` 内置 JSON 数据源，解析更稳定精准。
- **蓝桥云课 (Lanqiao)**：增加了 API 数据抓取机制，并针对需用户登录才能访问的题目增加了友好的优雅降级与指引提示。
- **Codeforces**：深度集成 Codeforces 官方 API，精准提取题目难度 (Rating) 及专题标签 (Tags)。

### 修复问题 (Bug Fixes)
- 修复了 Luogu 爬虫原有的 `HTTP 0` 网络异常错误判定漏洞，增强了系统底层 `CprHttpClient` 对于网络/代理错误的详细捕获及提示机制。
- 修正了 Codeforces 链接解析的正则表达式，解决了原本 ID 与题目简码提取错误、名称变异的问题。
- 修复了 Git 仓库的跟踪遗留问题，删除了旧有的构建残余、测试脚本、IDE 配置日志等，并深度优化了 `.gitignore`。
- 修复并优化了 GitHub Actions 的 `crawler_ci.yml`，统一使用更稳定高效的 `lukka/run-vcpkg` 缓存机制，解决了 Windows PowerShell 跨平台编译构建 CMAKE 参数解析出错的问题。
- 全面重写并修复了相关适配器的 Mock 拦截单元测试，适配最新爬虫流程。

### 破坏性变更 (Breaking Changes)
- 无

### 升级指引 (Upgrade Guide)
- 请直接在客户端内通过 `update` 命令进行获取，或从本 Release 下载最新安装程序 (`shuati-setup-x64-v0.0.2.exe`) 进行覆盖安装。

## [0.0.1] - 2026-02-26

### 新增

- **引导程序**：初始项目设置，统一版本为 v0.0.1。

- **构建**：修复了 CMake 配置和构建问题。

- **持续集成**：修复了 GitHub Actions CI/CD 工作流程。

- **核心功能**：

- 多平台爬虫：LeetCode (GraphQL)、Codeforces、罗谷、蓝桥

- 本地评判引擎：C++ 和 Python 的沙箱执行

- AI 教练：集成 LLM 用于错误诊断

- 记忆系统：SM2 算法用于间隔重复训练

- 带自动补全功能的交互式 REPL 模式
