# Shuati CLI Installer v1.4.2
$ErrorActionPreference = "Stop"

$InstallDir = "$env:LOCALAPPDATA\shuati-cli"
Write-Host "正在安装 Shuati CLI 到: $InstallDir ..." -ForegroundColor Cyan

if (!(Test-Path $InstallDir)) { 
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null 
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Get-LatestWindowsAssetUrls([string]$Repo) {
    $headers = @{
        "User-Agent" = "shuati-cli-installer"
        "Accept"     = "application/vnd.github+json"
    }
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -Headers $headers
    $assets = @($release.assets)
    $zip = $assets | Where-Object { $_.name -like "shuati-windows-x64-*.zip" } | Select-Object -First 1
    $sha = $assets | Where-Object { $_.name -like "shuati-windows-x64-*.zip.sha256" } | Select-Object -First 1
    if (-not $zip) { throw "未找到 Windows 安装包 zip（shuati-windows-x64-*.zip）" }
    return @{
        TagName = [string]$release.tag_name
        ZipName = [string]$zip.name
        ZipUrl  = [string]$zip.browser_download_url
        ShaUrl  = if ($sha) { [string]$sha.browser_download_url } else { "" }
    }
}

function Install-FromDirectory([string]$FromDir, [string]$ToDir) {
    Get-ChildItem $FromDir -Exclude "install.ps1" | Copy-Item -Destination $ToDir -Recurse -Force
}

if (Test-Path (Join-Path $ScriptDir "shuati.exe")) {
    Install-FromDirectory -FromDir $ScriptDir -ToDir $InstallDir
}
else {
    $repo = "Xustalis/shuati-Cli"
    $meta = Get-LatestWindowsAssetUrls -Repo $repo
    $tmpRoot = Join-Path $env:TEMP ("shuati-cli-install-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $tmpRoot | Out-Null
    $zipPath = Join-Path $tmpRoot $meta.ZipName
    Write-Host "正在下载: $($meta.ZipName) ($($meta.TagName)) ..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $meta.ZipUrl -OutFile $zipPath

    if ($meta.ShaUrl -and $meta.ShaUrl.Trim().Length -gt 0) {
        $shaPath = Join-Path $tmpRoot ($meta.ZipName + ".sha256")
        Invoke-WebRequest -Uri $meta.ShaUrl -OutFile $shaPath
        $expected = ((Get-Content $shaPath -Raw).Trim() -split "\s+")[0]
        $actual = (Get-FileHash -Algorithm SHA256 $zipPath).Hash.ToLowerInvariant()
        if ($expected.ToLowerInvariant() -ne $actual) {
            throw "SHA256 校验失败。expected=$expected actual=$actual"
        }
    }

    $extractDir = Join-Path $tmpRoot "extract"
    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force
    if (-not (Test-Path (Join-Path $extractDir "shuati.exe"))) {
        $candidates = Get-ChildItem $extractDir -Recurse -Filter "shuati.exe" | Select-Object -First 1
        if ($candidates) {
            $extractDir = $candidates.Directory.FullName
        }
    }
    if (-not (Test-Path (Join-Path $extractDir "shuati.exe"))) {
        throw "解压后未找到 shuati.exe"
    }

    Install-FromDirectory -FromDir $extractDir -ToDir $InstallDir
}

$UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($UserPath -notlike "*$InstallDir*") {
    [Environment]::SetEnvironmentVariable("Path", "$UserPath;$InstallDir", "User")
    Write-Host "[+] 已添加 $InstallDir 到用户 PATH 环境变量。" -ForegroundColor Green
    Write-Host "    注意: 您需要重启终端 (Terminal) 才能生效。" -ForegroundColor Yellow
}
else {
    Write-Host "[=] PATH 环境变量已配置。" -ForegroundColor Green
}

Write-Host "`n正在初始化..." -ForegroundColor Cyan
Start-Process "$InstallDir\shuati.exe" -ArgumentList "init" -NoNewWindow -Wait

Write-Host "`n安装完成！🎉" -ForegroundColor Green
Write-Host "请重启终端，然后输入 'shuati' 开始使用。" -ForegroundColor Cyan
