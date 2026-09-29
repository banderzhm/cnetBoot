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
& $git.Source -C $root submodule update --init --recursive --depth 1 -- 3rdparty/cnetmod
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not (Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt')) -or
    -not (Test-Path -LiteralPath (Join-Path $source '3rdparty/boringssl/CMakeLists.txt'))) {
    throw 'cnetmod submodules are incomplete: git submodule update --init --recursive --depth 1'
}
if ($Jobs -lt 1) { throw 'Jobs must be at least 1.' }

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path -LiteralPath $vswhere) {
    (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
}
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmake = $cmake.Source }
if (-not $cmake -and $vs) {
    $bundled = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
}
if (-not $cmake) { throw 'CMake was not found on PATH or in Visual Studio.' }

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
    '-DCNETMOD_USE_SYSTEM_DEPS=OFF',
    '-DCNETMOD_USE_MIMALLOC=OFF',
    '-DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE',
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"
)

[string[]]$freshArg = @()
if ($Fresh) { $freshArg += '--fresh' }
& $cmake @freshArg -S $source -B $BuildDir -A x64 @options
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $BuildDir --config $Configuration --target cnetmod_core --parallel $Jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --install $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$package = Join-Path $InstallPrefix 'lib/cmake/cnetmod/cnetmodConfig.cmake'
if (-not (Test-Path -LiteralPath $package)) {
    throw "cnetmod install is incomplete: $InstallPrefix"
}
Write-Output "Windows cnetmod installed at $InstallPrefix"
