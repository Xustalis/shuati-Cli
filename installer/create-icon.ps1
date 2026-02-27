#Requires -Version 5.1
<#
.SYNOPSIS
    Creates a simple icon for Shuati CLI installer
.DESCRIPTION
    This script creates a simple ICO file for the Shuati CLI installer.
    In production, you should replace this with your actual application icon.
#>

[CmdletBinding()]
param(
    [string]$OutputPath = "$PSScriptRoot\assets\icon.ico"
)

# Try to use ImageMagick if available
$Magick = Get-Command "magick" -ErrorAction SilentlyContinue
if ($Magick) {
    Write-Host "Using ImageMagick to create icon..."
    
    # Create a simple colored square icon
    $Sizes = @(16, 32, 48, 64, 128, 256)
    $TempFiles = @()
    
    foreach ($Size in $Sizes) {
        $TempFile = "$env:TEMP\icon_$Size.png"
        $TempFiles += $TempFile
        
        # Create a blue gradient square with "S" text
        & magick convert -size "$($Size)x$($Size)" "xc:#2196F3" -pointsize ($Size/2) -fill white -gravity center -annotate +0+0 "S" $TempFile
    }
    
    # Combine into ICO
    & magick convert $TempFiles $OutputPath
    
    # Cleanup
    $TempFiles | Remove-Item -ErrorAction SilentlyContinue
    
    Write-Host "Icon created: $OutputPath" -ForegroundColor Green
    return
}

# Try to use System.Drawing
Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue

if ([System.Drawing.Bitmap] -ne $null) {
    Write-Host "Using System.Drawing to create icon..."
    
    # Create a memory stream for the ICO file
    $MemoryStream = New-Object System.IO.MemoryStream
    $BinaryWriter = New-Object System.IO.BinaryWriter($MemoryStream)
    
    # ICO Header
    $BinaryWriter.Write([byte]0)     # Reserved
    $BinaryWriter.Write([byte]0)
    $BinaryWriter.Write([byte]1)     # Type (1 = ICO)
    $BinaryWriter.Write([byte]0)
    $BinaryWriter.Write([byte]1)     # Count (1 image)
    $BinaryWriter.Write([byte]0)
    
    # ICONDIRENTRY
    $BinaryWriter.Write([byte]32)    # Width
    $BinaryWriter.Write([byte]32)    # Height
    $BinaryWriter.Write([byte]0)     # Colors
    $BinaryWriter.Write([byte]0)     # Reserved
    $BinaryWriter.Write([byte]1)     # Color planes
    $BinaryWriter.Write([byte]0)
    $BinaryWriter.Write([byte]32)    # Bits per pixel
    $BinaryWriter.Write([byte]0)
    
    # Create a simple 32x32 bitmap
    $Bitmap = New-Object System.Drawing.Bitmap 32, 32
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    
    # Fill with blue background
    $Graphics.Clear([System.Drawing.Color]::FromArgb(33, 150, 243))
    
    # Draw "S" text
    $Font = New-Object System.Drawing.Font "Arial", 16, [System.Drawing.FontStyle]::Bold
    $Brush = [System.Drawing.Brushes]::White
    $Graphics.DrawString("S", $Font, $Brush, 6, 2)
    
    $Graphics.Dispose()
    
    # Save bitmap to memory
    $BitmapStream = New-Object System.IO.MemoryStream
    $Bitmap.Save($BitmapStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $BitmapData = $BitmapStream.ToArray()
    
    # Write size and offset
    $BinaryWriter.Write([int]$BitmapData.Length)
    $BinaryWriter.Write([int]22)  # Offset to image data
    
    # Write image data
    $BinaryWriter.Write($BitmapData)
    
    # Save to file
    $Directory = Split-Path -Parent $OutputPath
    if (-not (Test-Path $Directory)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }
    
    [System.IO.File]::WriteAllBytes($OutputPath, $MemoryStream.ToArray())
    
    $BinaryWriter.Close()
    $MemoryStream.Close()
    $BitmapStream.Close()
    $Bitmap.Dispose()
    
    Write-Host "Icon created: $OutputPath" -ForegroundColor Green
    Write-Host "Note: This is a placeholder icon. Please replace it with your actual application icon." -ForegroundColor Yellow
    return
}

# Fallback: create an empty file and warn
Write-Warning "Could not create icon. Neither ImageMagick nor System.Drawing is available."
Write-Host "Creating placeholder file..."

$Directory = Split-Path -Parent $OutputPath
if (-not (Test-Path $Directory)) {
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
}

# Create a minimal valid ICO file (1x1 pixel)
$IcoBytes = @(
    0x00, 0x00, # Reserved
    0x01, 0x00, # Type (ICO)
    0x01, 0x00, # Count
    0x01,       # Width
    0x01,       # Height
    0x00,       # Colors
    0x00,       # Reserved
    0x01, 0x00, # Color planes
    0x20, 0x00, # Bits per pixel
    0x28, 0x00, 0x00, 0x00, # Size
    0x16, 0x00, 0x00, 0x00, # Offset
    # BMP header
    0x28, 0x00, 0x00, 0x00, # Header size
    0x01, 0x00, 0x00, 0x00, # Width
    0x02, 0x00, 0x00, 0x00, # Height (doubled for XOR and AND masks)
    0x01, 0x00,             # Planes
    0x20, 0x00,             # Bits per pixel
    0x00, 0x00, 0x00, 0x00, # Compression
    0x00, 0x00, 0x00, 0x00, # Image size
    0x00, 0x00, 0x00, 0x00, # X pixels per meter
    0x00, 0x00, 0x00, 0x00, # Y pixels per meter
    0x00, 0x00, 0x00, 0x00, # Colors used
    0x00, 0x00, 0x00, 0x00, # Important colors
    # Pixel data (1 blue pixel)
    0xFF, 0x00, 0x00, 0xFF,
    # Mask (fully opaque)
    0x00, 0x00, 0x00, 0x00
)

[System.IO.File]::WriteAllBytes($OutputPath, [byte[]]$IcoBytes)
Write-Host "Placeholder icon created: $OutputPath" -ForegroundColor Yellow
Write-Host "IMPORTANT: Please replace this with your actual application icon!" -ForegroundColor Red
