# Shuati CLI Installer v0.0.2
$ErrorActionPreference = "Stop"

$InstallDir = "$env:LOCALAPPDATA\shuati-cli"
Write-Host "正在安装 Shuati CLI 到: $InstallDir ..." -ForegroundColor Cyan

if (!(Test-Path $InstallDir)) { 
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null 
}

if ($MyInvocation.MyCommand.Path) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
else {
    $ScriptDir = $PWD
}

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
# Environment Variable Update Logic
try {
    $reg = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey("Environment", $true)
    $rawPath = $reg.GetValue("Path", "", [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    
    # Normalize paths for comparison (remove trailing slashes, case insensitive)
    $normInstallDir = $InstallDir.TrimEnd('\')
    
    $pathParts = $rawPath -split ';'
    $alreadyExists = $false
    foreach ($part in $pathParts) {
        if ($part.Trim().TrimEnd('\') -eq $normInstallDir) {
            $alreadyExists = $true
            break
        }
    }

    if (-not $alreadyExists) {
        if ([string]::IsNullOrWhiteSpace($rawPath)) {
            $newPath = $InstallDir
        }
        else {
            $newPath = "$rawPath;$InstallDir"
        }
        
        $reg.SetValue("Path", $newPath, [Microsoft.Win32.RegistryValueKind]::ExpandString)
        $reg.Close()
        
        Write-Host "[+] Verified: Path updated in Registry." -ForegroundColor Green
        
        # Broadcast WM_SETTINGCHANGE
        $code = @"
using System;
using System.Runtime.InteropServices;
public class EnvUpdate {
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessageTimeout(
        IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam,
        uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
}
"@
        try {
            Add-Type -TypeDefinition $code -Name "EnvUpdate" -Namespace "ShuatiInstaller" -MemberDefinition "" -ErrorAction SilentlyContinue
            $HWND_BROADCAST = [IntPtr]0xffff
            $WM_SETTINGCHANGE = 0x001A
            $SMTO_ABORTIFHUNG = 0x0002
            $result = [UIntPtr]::Zero
            
            [ShuatiInstaller.EnvUpdate]::SendMessageTimeout(
                $HWND_BROADCAST, $WM_SETTINGCHANGE, [UIntPtr]::Zero, "Environment",
                $SMTO_ABORTIFHUNG, 5000, [ref]$result
            ) | Out-Null
            Write-Host "[+] Environment change broadcasted." -ForegroundColor Green
        }
        catch {
            Write-Warning "Could not broadcast environment change. A restart fits best."
        }
        
        Write-Host "    注意: 为了让更改在当前终端生效，您可能需要重启终端。" -ForegroundColor Yellow
    }
    else {
        $reg.Close()
        Write-Host "[=] PATH 环境变量已配置 (Verified in Registry)." -ForegroundColor Green
    }
}
catch {
    Write-Error "Failed to update PATH in Registry: $_"
}

Write-Host "`n正在初始化..." -ForegroundColor Cyan
Start-Process "$InstallDir\shuati.exe" -ArgumentList "init" -NoNewWindow -Wait

Write-Host "`n安装完成！🎉" -ForegroundColor Green
Write-Host "请重启终端，然后输入 'shuati' 开始使用。" -ForegroundColor Cyan
