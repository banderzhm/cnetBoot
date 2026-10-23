param(
    [string]$BuildDir,
    [string]$Configuration = 'Release',
    [int]$Jobs = 1,
    [switch]$BuildTests,
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sourceDir = Join-Path $root 'backend'
$prefix = Join-Path $root '3rdparty/windows'
if (-not $BuildDir) { $BuildDir = Join-Path $sourceDir 'build-windows' }
if ($Jobs -lt 1) { throw 'Jobs must be at least 1.' }

$config = Join-Path $prefix 'lib/cmake/cnetmod/cnetmodConfig.cmake'
if (-not (Test-Path -LiteralPath $config)) {
    throw "cnetmod is not installed. Run .\script\windows_3rdparty_build.ps1 first. Missing: $config"
}
if (-not (Get-ChildItem -LiteralPath (Join-Path $prefix 'lib') -Filter '*.lib' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw "The cnetmod install contains no Windows libraries: $prefix"
}

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path -LiteralPath $vswhere) {
    (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
}
if (-not $vs) { throw 'Visual Studio with the MSVC x64 toolchain was not found.' }

$vsDevCmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "Visual Studio developer environment was not found: $vsDevCmd"
}
& $env:COMSPEC /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set" |
    ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item -LiteralPath "Env:$($matches[1])" -Value $matches[2]
        }
    }
if ($LASTEXITCODE -ne 0) { throw 'Failed to initialize the Visual Studio x64 environment.' }

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmake = $cmake.Source }
if (-not $cmake) {
    $bundled = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
}
if (-not $cmake) { throw 'CMake was not found on PATH or in Visual Studio.' }
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninja) { $ninja = $ninja.Source }
if (-not $ninja) {
    $bundled = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    if (Test-Path -LiteralPath $bundled) { $ninja = $bundled }
}
if (-not $ninja) { throw 'Ninja was not found on PATH or in Visual Studio.' }

$testing = if ($BuildTests) { 'ON' } else { 'OFF' }
[string[]]$freshArg = @()
if ($Fresh) { $freshArg += '--fresh' }
& $cmake @freshArg -S $sourceDir -B $BuildDir -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    '-DCMAKE_C_COMPILER=cl' `
    '-DCMAKE_CXX_COMPILER=cl' `
    "-DBUILD_TESTING=$testing" `
    "-DCMAKE_PREFIX_PATH=$prefix" `
    "-Dcnetmod_DIR=$($prefix.Replace('\', '/'))/lib/cmake/cnetmod"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $BuildDir --parallel $Jobs
exit $LASTEXITCODE
