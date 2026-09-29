param(
    [string]$BuildDir,
    [string]$Configuration = 'Release',
    [int]$Jobs = 1,
    [switch]$BuildTests
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

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmake = $cmake.Source }
if (-not $cmake) {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vs = (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
        if ($vs) {
            $bundled = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
        }
    }
}
if (-not $cmake) { throw 'CMake was not found on PATH or in Visual Studio.' }

$testing = if ($BuildTests) { 'ON' } else { 'OFF' }
& $cmake -S $sourceDir -B $BuildDir -A x64 "-DBUILD_TESTING=$testing" `
    "-DCMAKE_PREFIX_PATH=$prefix" `
    "-Dcnetmod_DIR=$($prefix.Replace('\', '/'))/lib/cmake/cnetmod"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $BuildDir --config $Configuration --parallel $Jobs
exit $LASTEXITCODE
