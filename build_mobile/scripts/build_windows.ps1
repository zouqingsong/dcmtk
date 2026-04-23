# build_windows.ps1 — Build DCMTK static libraries for Windows (x64)
#
# Prerequisites:
#   - Visual Studio 2019 or later with C++ desktop workload
#   - CMake 3.15+ (included with VS or standalone)
#
# Usage:
#   .\build_windows.ps1
#
# Output:
#   flutter_plugin/windows/Libs/*.lib    — DCMTK static libraries
#   flutter_plugin/windows/Headers/      — DCMTK headers

param(
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$WithOpenSSL,
    [string]$OpenSSLRoot = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DcmtkRoot = (Resolve-Path "$ScriptDir\..\..\").Path
$BuildDir = Join-Path $DcmtkRoot "build_windows"
$PluginDir = Join-Path $DcmtkRoot "flutter_plugin\windows"
$LibsDir = Join-Path $PluginDir "Libs"
$HeadersDir = Join-Path $PluginDir "Headers"

# Build both Release and Debug so flutter run (Debug) and flutter build (Release) both work
$BuildTypes = @("Release", "Debug")

Write-Host "=== DCMTK Windows Build ===" -ForegroundColor Cyan
Write-Host "DCMTK Root:  $DcmtkRoot"
Write-Host "Build Dir:   $BuildDir"
Write-Host "Output Libs: $LibsDir"
Write-Host "Output Hdrs: $HeadersDir"
Write-Host "Configs:     $($BuildTypes -join ', ')"
Write-Host "Generator:   $Generator"
Write-Host ""

# Clean previous build artifacts
if (Test-Path $BuildDir) {
    Write-Host "Cleaning previous build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

# Clean previous output
foreach ($dir in @($LibsDir, $HeadersDir)) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
    }
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

# Create per-config lib directories
foreach ($bt in $BuildTypes) {
    New-Item -ItemType Directory -Path (Join-Path $LibsDir $bt) -Force | Out-Null
}

# CMake configure
Write-Host "`n=== CMake Configure ===" -ForegroundColor Cyan
$cmakeArgs = @(
    "-S", $DcmtkRoot,
    "-B", $BuildDir,
    "-G", $Generator,
    "-A", "x64",
    "-DBUILD_SHARED_LIBS=OFF",
    "-DBUILD_APPS=OFF",
    "-DBUILD_TESTING=OFF",
    "-DDCMTK_COMPILE_WIN32_MULTITHREADED_DLL=ON",
    "-DDCMTK_WITH_XML=OFF",
    "-DDCMTK_WITH_PNG=OFF",
    "-DDCMTK_WITH_TIFF=OFF",
    "-DDCMTK_WITH_SNDFILE=OFF",
    "-DDCMTK_WITH_WRAP=OFF",
    "-DDCMTK_WITH_ICONV=OFF",
    "-DDCMTK_WITH_DOXYGEN=OFF",
    "-DDCMTK_ENABLE_CHARSET_CONVERSION=OFF",
    "-DDCMTK_WITH_ZLIB=ON",
    "-DDCMTK_WITH_THREADS=ON"
)

if ($WithOpenSSL -and $OpenSSLRoot) {
    $cmakeArgs += @(
        "-DDCMTK_WITH_OPENSSL=ON",
        "-DOPENSSL_ROOT_DIR=$OpenSSLRoot"
    )
} else {
    $cmakeArgs += "-DDCMTK_WITH_OPENSSL=OFF"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# CMake build (both Release and Debug)
foreach ($bt in $BuildTypes) {
    Write-Host "`n=== CMake Build ($bt) ===" -ForegroundColor Cyan
    & cmake --build $BuildDir --config $bt --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build ($bt) failed" }
}

# Collect static libraries (per-config)
Write-Host "`n=== Collecting Libraries ===" -ForegroundColor Cyan
foreach ($bt in $BuildTypes) {
    $configLibsDir = Join-Path $LibsDir $bt
    $libs = Get-ChildItem -Path $BuildDir -Recurse -Filter "*.lib" | Where-Object {
        $_.FullName -notmatch "CMakeFiles" -and
        $_.FullName -notmatch "INSTALL" -and
        $_.DirectoryName -match "\\$bt$"
    }
    foreach ($lib in $libs) {
        $destPath = Join-Path $configLibsDir $lib.Name
        Copy-Item $lib.FullName -Destination $destPath -Force
        Write-Host "  Copied ($bt): $($lib.Name)"
    }
}

# Collect headers
Write-Host "`n=== Collecting Headers ===" -ForegroundColor Cyan

# Copy generated osconfig.h
$osconfig = Join-Path $BuildDir "config\include\dcmtk\config\osconfig.h"
if (Test-Path $osconfig) {
    $destDir = Join-Path $HeadersDir "dcmtk\config"
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    Copy-Item $osconfig -Destination $destDir -Force
    Write-Host "  Copied: dcmtk/config/osconfig.h"
}

# Copy module headers
$modules = @(
    "ofstd", "oflog", "oficonv", "dcmdata", "dcmimgle", "dcmimage",
    "dcmjpeg", "dcmjpls", "dcmtls", "dcmnet", "dcmsr", "dcmsign",
    "dcmwlm", "dcmqrdb", "dcmpstat", "dcmrt", "dcmiod", "dcmfg",
    "dcmseg", "dcmtract", "dcmpmap", "dcmect"
)

foreach ($module in $modules) {
    $srcDir = Join-Path $DcmtkRoot "$module\include"
    if (Test-Path $srcDir) {
        # Copy entire include tree
        $items = Get-ChildItem -Path $srcDir -Recurse -File
        foreach ($item in $items) {
            $relativePath = $item.FullName.Substring($srcDir.Length + 1)
            $destPath = Join-Path $HeadersDir $relativePath
            $destDir = Split-Path $destPath -Parent
            if (!(Test-Path $destDir)) {
                New-Item -ItemType Directory -Path $destDir -Force | Out-Null
            }
            Copy-Item $item.FullName -Destination $destPath -Force
        }
        Write-Host "  Copied headers: $module"
    }
}

# Summary
$releaseCount = (Get-ChildItem -Path (Join-Path $LibsDir "Release") -Filter "*.lib").Count
$debugCount = (Get-ChildItem -Path (Join-Path $LibsDir "Debug") -Filter "*.lib").Count
Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "Libraries: $releaseCount Release + $debugCount Debug .lib files in $LibsDir"
Write-Host "Headers:   $HeadersDir"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open medview in VS Code"
Write-Host "  2. Run: flutter run -d windows"
