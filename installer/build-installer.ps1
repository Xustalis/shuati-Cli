#Requires -Version 5.1
<#
.SYNOPSIS
    Build Shuati CLI Windows Installer
.DESCRIPTION
    This script builds the Windows installer for Shuati CLI using Inno Setup.
    It handles version detection, file preparation, and installer compilation.
.PARAMETER Version
    The version number for the installer. If not specified, uses the version from CMakeLists.txt or defaults to "0.0.1".
.PARAMETER SourceDir
    The directory containing the built executable and resources. Defaults to "..\build\Release".
.PARAMETER OutputDir
    The directory where the installer will be placed. Defaults to "..\dist".
.PARAMETER SkipBuild
    Skip the build step and use existing files.
.EXAMPLE
    .\build-installer.ps1 -Version "1.0.0"
    .\build-installer.ps1 -SourceDir "..\build\Release" -OutputDir "..\dist"
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$SourceDir = "..\build\Release",
    [string]$OutputDir = "..\dist",
    [switch]$SkipBuild
)

# Error handling
$ErrorActionPreference = "Stop"

# Script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Shuati CLI Installer Builder" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Detect or validate version
Write-Host "[Step 1/5] Version Detection" -ForegroundColor Yellow

if ([string]::IsNullOrWhiteSpace($Version)) {
    # Try to get version from CMakeLists.txt
    $CMakeFile = Join-Path $ProjectRoot "CMakeLists.txt"
    if (Test-Path $CMakeFile) {
        $Content = Get-Content $CMakeFile -Raw
        if ($Content -match 'project\s*\(\s*[^\)]+VERSION\s+(\d+\.\d+\.\d+)') {
            $Version = $Matches[1]
            Write-Host "  Detected version from CMakeLists.txt: $Version" -ForegroundColor Green
        }
    }
    
    if ([string]::IsNullOrWhiteSpace($Version)) {
        $Version = "0.0.1"
        Write-Host "  Using default version: $Version" -ForegroundColor Yellow
    }
} else {
    Write-Host "  Using specified version: $Version" -ForegroundColor Green
}

# Step 2: Verify source files exist
Write-Host ""
Write-Host "[Step 2/5] Source File Verification" -ForegroundColor Yellow

$FullSourceDir = Join-Path $ScriptDir $SourceDir
if (-not (Test-Path $FullSourceDir)) {
    Write-Error "Source directory not found: $FullSourceDir"
    exit 1
}

$Executable = Join-Path $FullSourceDir "shuati.exe"
if (-not (Test-Path $Executable)) {
    Write-Error "Executable not found: $Executable"
    Write-Host "  Please build the project first using: cmake --build build --config Release" -ForegroundColor Red
    exit 1
}

Write-Host "  Executable found: $Executable" -ForegroundColor Green

# Check for resources
$ResourceDir = Join-Path $FullSourceDir "resources"
if (Test-Path $ResourceDir) {
    Write-Host "  Resources found: $ResourceDir" -ForegroundColor Green
} else {
    Write-Host "  Warning: Resources directory not found" -ForegroundColor Yellow
}

# Step 3: Prepare output directory
Write-Host ""
Write-Host "[Step 3/5] Preparing Output Directory" -ForegroundColor Yellow

$FullOutputDir = Join-Path $ProjectRoot $OutputDir
if (-not (Test-Path $FullOutputDir)) {
    New-Item -ItemType Directory -Force -Path $FullOutputDir | Out-Null
    Write-Host "  Created output directory: $FullOutputDir" -ForegroundColor Green
} else {
    Write-Host "  Output directory exists: $FullOutputDir" -ForegroundColor Green
}

# Step 4: Find Inno Setup Compiler
Write-Host ""
Write-Host "[Step 4/5] Finding Inno Setup Compiler" -ForegroundColor Yellow

$ISCCPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 5\ISCC.exe",
    "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
)

$ISCC = $null
foreach ($Path in $ISCCPaths) {
    if (Test-Path $Path) {
        $ISCC = $Path
        break
    }
}

if (-not $ISCC) {
    # Try to find in PATH
    $ISCC = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $ISCC) {
    Write-Error @"
Inno Setup Compiler (ISCC.exe) not found!
Please install Inno Setup from: https://jrsoftware.org/isdl.php
"@
    exit 1
}

Write-Host "  Found ISCC: $ISCC" -ForegroundColor Green

# Step 5: Build installer
Write-Host ""
Write-Host "[Step 5/5] Building Installer" -ForegroundColor Yellow

$ISSFile = Join-Path $ScriptDir "shuati.iss"
if (-not (Test-Path $ISSFile)) {
    Write-Error "ISS file not found: $ISSFile"
    exit 1
}

# Set environment variables for the ISS script
$env:SHUATI_VERSION = $Version
$env:SHUATI_OUTPUT_NAME = "shuati-cli-$Version-setup"
$env:SHUATI_SOURCE_DIR = $FullSourceDir

Write-Host "  Version: $Version" -ForegroundColor Cyan
Write-Host "  Source: $FullSourceDir" -ForegroundColor Cyan
Write-Host "  Output: $FullOutputDir" -ForegroundColor Cyan
Write-Host ""

# Create a simple icon if it doesn't exist
$IconFile = Join-Path $ScriptDir "assets\icon.ico"
if (-not (Test-Path $IconFile)) {
    Write-Host "  Creating default icon..." -ForegroundColor Yellow
    # Create a simple icon using PowerShell (this is a placeholder - in production you'd use a real icon)
    # For now, we'll copy the executable's icon or create a blank file
    "" | Set-Content -Path $IconFile -NoNewline
    Write-Host "  Note: Please replace assets\icon.ico with your actual application icon" -ForegroundColor Yellow
}

# Run ISCC
try {
    $Arguments = @(
        "`"$ISSFile`""
        "/O`"$FullOutputDir`""
        "/F`"shuati-cli-$Version-setup`""
    )
    
    Write-Host "  Running: ISCC $Arguments" -ForegroundColor DarkGray
    & $ISCC $Arguments
    
    if ($LASTEXITCODE -ne 0) {
        throw "ISCC exited with code $LASTEXITCODE"
    }
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Build Successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    
    $InstallerPath = Join-Path $FullOutputDir "shuati-cli-$Version-setup.exe"
    if (Test-Path $InstallerPath) {
        $FileInfo = Get-Item $InstallerPath
        Write-Host "Installer: $InstallerPath" -ForegroundColor Cyan
        Write-Host "Size: $([math]::Round($FileInfo.Length / 1MB, 2)) MB" -ForegroundColor Cyan
        Write-Host ""
    }
} catch {
    Write-Error "Failed to build installer: $_"
    exit 1
}

# Cleanup
Remove-Item Env:\SHUATI_VERSION -ErrorAction SilentlyContinue
Remove-Item Env:\SHUATI_OUTPUT_NAME -ErrorAction SilentlyContinue
Remove-Item Env:\SHUATI_SOURCE_DIR -ErrorAction SilentlyContinue

Write-Host "Done!" -ForegroundColor Green
