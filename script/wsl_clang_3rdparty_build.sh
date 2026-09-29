#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cnetmod_source="$root/3rdparty/cnetmod"
build_dir="${CNETBOOT_3RDPARTY_BUILD_DIR:-$root/3rdparty/build-wsl-clang}"
prefix="${CNETBOOT_3RDPARTY_PREFIX:-$root/3rdparty/wsl-clang}"
jobs="${CNETBOOT_JOBS:-2}"

for tool in git cmake ninja clang clang++; do
    command -v "$tool" >/dev/null || { echo "Missing required tool: $tool" >&2; exit 1; }
done
git -C "$root" submodule update --init --recursive --depth 1 -- 3rdparty/cnetmod
[[ -f "$cnetmod_source/CMakeLists.txt" && \
   -f "$cnetmod_source/3rdparty/boringssl/CMakeLists.txt" ]] || {
    echo "cnetmod submodules are incomplete: git submodule update --init --recursive --depth 1" >&2
    exit 1
}

fresh=()
if [[ "${CNETBOOT_FRESH:-0}" == 1 ]]; then fresh+=(--fresh); fi
cmake "${fresh[@]}" -S "$cnetmod_source" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(command -v clang)" \
    -DCMAKE_CXX_COMPILER="$(command -v clang++)" \
    -DCMAKE_ASM_COMPILER="$(command -v clang)" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCNETMOD_ENABLE_ALL_PROTOCOLS=OFF \
    -DCNETMOD_ENABLE_HTTP=ON \
    -DCNETMOD_ENABLE_MYSQL=ON \
    -DCNETMOD_ENABLE_REDIS=ON \
    -DCNETMOD_ENABLE_OPENAI=ON \
    -DCNETMOD_ENABLE_ORM=ON \
    -DCNETMOD_BUILD_C_API=OFF \
    -DCNETMOD_BUILD_PYTHON_BINDINGS=OFF \
    -DCNETMOD_BUILD_EXAMPLES=OFF \
    -DCNETMOD_BUILD_TESTS=OFF \
    -DCNETMOD_BUILD_BENCH=OFF \
    -DCNETMOD_USE_SYSTEM_DEPS=OFF \
    -DCNETMOD_USE_MIMALLOC=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE
cmake --build "$build_dir" --target cnetmod_core --parallel "$jobs"
cmake --install "$build_dir"

[[ -f "$prefix/lib/cmake/cnetmod/cnetmodConfig.cmake" ]] || {
    echo "cnetmod install is incomplete: $prefix" >&2
    exit 1
}
echo "WSL cnetmod installed at $prefix"
