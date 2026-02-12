# Shuati CLI Installer v1.3.1
$ErrorActionPreference = "Stop"

$InstallDir = "$env:LOCALAPPDATA\shuati-cli"
Write-Host "正在安装 Shuati CLI 到: $InstallDir ..." -ForegroundColor Cyan

# 1. 创建安装目录
if (!(Test-Path $InstallDir)) { 
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null 
}

# 2. 复制文件
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Get-ChildItem $ScriptDir -Exclude "install.ps1" | Copy-Item -Destination $InstallDir -Recurse -Force

# 3. 添加到 PATH
$UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($UserPath -notlike "*$InstallDir*") {
    [Environment]::SetEnvironmentVariable("Path", "$UserPath;$InstallDir", "User")
    Write-Host "[+] 已添加 $InstallDir 到用户 PATH 环境变量。" -ForegroundColor Green
    Write-Host "    注意: 您需要重启终端 (Terminal) 才能生效。" -ForegroundColor Yellow
}
else {
    Write-Host "[=] PATH 环境变量已配置。" -ForegroundColor Green
}

# 4. 初始化
Write-Host "`n正在初始化..." -ForegroundColor Cyan
Start-Process "$InstallDir\shuati.exe" -ArgumentList "init" -NoNewWindow -Wait

Write-Host "`n安装完成！🎉" -ForegroundColor Green
Write-Host "请重启终端，然后输入 'shuati' 开始使用。" -ForegroundColor Cyan
