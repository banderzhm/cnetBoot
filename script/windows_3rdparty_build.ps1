param(
    [string]$BuildDir,
    [string]$InstallPrefix,
    [string]$Configuration = 'Release',
    [int]$Jobs = 1,
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $root '3rdparty/cnetmod'
if (-not $BuildDir) { $BuildDir = Join-Path $root '3rdparty/build-windows' }
if (-not $InstallPrefix) { $InstallPrefix = Join-Path $root '3rdparty/windows' }

$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) {
    throw 'Git is required to initialize 3rdparty/cnetmod and its nested submodules.'
}
& $git.Source -C $root submodule update --init --depth 1 -- 3rdparty/cnetmod
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Only initialize dependencies required by the enabled cnetmod components.
# ICU belongs to PostgreSQL and LevelDB belongs to Raft; pulling every nested
# submodule makes this HTTP/MySQL/Redis/ORM build slower and less reliable.
$dependencies = @(
    '3rdparty/boringssl',
    '3rdparty/json',
    '3rdparty/pugixml',
    '3rdparty/stdexec',
    '3rdparty/yaml-cpp',
    '3rdparty/yaml-cpp-modules'
)
& $git.Source -C $source submodule update --init --depth 1 -- @dependencies
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$requiredFiles = @(
    'CMakeLists.txt',
    '3rdparty/boringssl/CMakeLists.txt',
    '3rdparty/json/include/nlohmann/json.hpp',
    '3rdparty/pugixml/CMakeLists.txt',
    '3rdparty/stdexec/include/exec/static_thread_pool.hpp',
    '3rdparty/yaml-cpp/CMakeLists.txt',
    '3rdparty/yaml-cpp-modules/CMakeLists.txt'
)
foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $relativePath))) {
        throw "cnetmod dependency is incomplete: $relativePath"
    }
}
if ($Jobs -lt 1) { throw 'Jobs must be at least 1.' }

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
if (-not $cmake -and $vs) {
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

$options = @(
    '-DCNETMOD_ENABLE_ALL_PROTOCOLS=OFF',
    '-DCNETMOD_ENABLE_HTTP=ON',
    '-DCNETMOD_ENABLE_MYSQL=ON',
    '-DCNETMOD_ENABLE_REDIS=ON',
    '-DCNETMOD_ENABLE_OPENAI=ON',
    '-DCNETMOD_ENABLE_ORM=ON',
    '-DCNETMOD_BUILD_C_API=OFF',
    '-DCNETMOD_BUILD_PYTHON_BINDINGS=OFF',
    '-DCNETMOD_BUILD_EXAMPLES=OFF',
    '-DCNETMOD_BUILD_TESTS=OFF',
    '-DCNETMOD_BUILD_BENCH=OFF',
    '-DCNETMOD_DEPENDENCY_MODE=system',
    '-DCNETMOD_USE_SYSTEM_DEPS=OFF',
    '-DCNETMOD_USE_MIMALLOC=OFF',
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"
)

[string[]]$freshArg = @()
if ($Fresh) { $freshArg += '--fresh' }
& $cmake @freshArg -S $source -B $BuildDir -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    '-DCMAKE_C_COMPILER=cl' `
    '-DCMAKE_CXX_COMPILER=cl' @options
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $BuildDir --target cnetmod_core --parallel $Jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --install $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$package = Join-Path $InstallPrefix 'lib/cmake/cnetmod/cnetmodConfig.cmake'
if (-not (Test-Path -LiteralPath $package)) {
    throw "cnetmod install is incomplete: $InstallPrefix"
}
Write-Output "Windows cnetmod installed at $InstallPrefix"
