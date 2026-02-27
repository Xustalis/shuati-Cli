# Shuati CLI 安装包制作指南

## 概述

本文档说明如何使用ISS文件创建Shuati CLI的Windows安装程序。

## 安装要求

### 必需软件

1. **Inno Setup 6** - Windows安装程序制作工具
   - 下载地址: https://jrsoftware.org/isdl.php
   - 安装时请选择安装编译器(ISCC)

2. **已构建的Shuati CLI可执行文件**
   - 位置: `..\build\Release\shuati.exe`
   - 资源文件: `..\build\Release\resources\`

## 快速开始

### 方法一：使用PowerShell脚本（推荐）

```powershell
# 进入installer目录
cd installer

# 创建图标（可选，首次运行时需要）
.\create-icon.ps1

# 构建安装包
.\build-installer.ps1

# 或者指定版本号
.\build-installer.ps1 -Version "1.0.0"
```

### 方法二：使用Inno Setup GUI

1. 打开Inno Setup Compiler
2. 选择 `File` → `Open`，打开 `shuati.iss`
3. 点击 `Build` → `Compile` 或按 `F9`
4. 生成的安装包位于 `..\dist\` 目录

### 方法三：使用命令行编译

```cmd
# 设置环境变量（可选）
set SHUATI_VERSION=0.0.1
set SHUATI_SOURCE_DIR=..\build\Release

# 编译
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" shuati.iss
```

## ISS文件优化说明

### 主要改进

1. **多语言支持**
   - 英文（默认）
   - 简体中文
   - 可扩展更多语言

2. **安装类型**
   - **完整安装** - 包含所有组件
   - **精简安装** - 仅核心功能
   - **自定义安装** - 用户选择组件

3. **安装组件**
   - 主程序（必需）
   - 资源文件（编译器规则等）
   - 文档（README、CHANGELOG等）

4. **安装任务**
   - 添加到PATH环境变量
   - 添加到右键菜单
   - 创建桌面快捷方式
   - 安装后显示帮助

5. **高级功能**
   - 自动版本检测（从CMakeLists.txt）
   - 压缩优化（lzma2/ultra64）
   - Windows应用注册
   - 智能PATH管理

### 文件结构

```
installer/
├── shuati.iss              # 主ISS脚本
├── build-installer.ps1     # PowerShell构建脚本
├── create-icon.ps1         # 图标创建脚本
├── assets/
│   └── icon.ico            # 安装程序图标
└── README.md               # 本文件
```

## 环境变量

ISS脚本支持以下环境变量：

| 变量名 | 说明 | 默认值 |
|--------|------|--------|
| `SHUATI_VERSION` | 版本号 | 从CMakeLists.txt读取或"0.0.1" |
| `SHUATI_OUTPUT_NAME` | 输出文件名 | `shuati-cli-{version}-setup` |
| `SHUATI_SOURCE_DIR` | 源文件目录 | `..\build\Release` |

## 自定义安装程序

### 修改版本号

编辑 `shuati.iss` 第16行：
```pascal
#define MyAppVersion "0.0.1"
```

### 添加新组件

在 `[Components]` 段添加：
```pascal
Name: "mycomponent"; Description: "My Component"; Types: full
```

在 `[Files]` 段添加对应文件：
```pascal
Source: "..\myfile.exe"; DestDir: "{app}"; Components: mycomponent
```

### 添加新任务

在 `[Tasks]` 段添加：
```pascal
Name: "mytask"; Description: "My Task"; GroupDescription: "Additional tasks:"
```

在 `[Code]` 段添加处理逻辑。

### 更换图标

替换 `assets\icon.ico` 文件，建议使用包含多种尺寸的ICO文件（16x16, 32x32, 48x48, 256x256）。

## 故障排除

### 问题：找不到ISCC.exe

**解决**: 安装Inno Setup并确保安装了编译器组件，或手动指定路径：
```powershell
$env:PATH += ";C:\Program Files (x86)\Inno Setup 6"
```

### 问题：找不到源文件

**解决**: 确保已构建项目：
```powershell
cmake --build build --config Release
```

### 问题：图标创建失败

**解决**: 手动创建一个 `assets\icon.ico` 文件，或使用ImageMagick：
```bash
magick convert -size 256x256 xc:blue assets/icon.ico
```

## 输出文件

成功构建后，安装包将位于：
```
dist/
└── shuati-cli-{version}-setup.exe
```

## CI/CD集成

在GitHub Actions中使用：

```yaml
- name: Build Installer
  run: |
    cd installer
    .\build-installer.ps1 -Version ${{ github.ref_name }}
  shell: pwsh

- name: Upload Installer
  uses: actions/upload-artifact@v4
  with:
    name: installer
    path: dist/*.exe
```

## 参考

- [Inno Setup文档](https://jrsoftware.org/ishelp/)
- [Inno Setup预处理器](https://jrsoftware.org/ispphelp/)
- [Pascal脚本参考](https://jrsoftware.org/ishelp/topic_scriptfunctions.htm)
