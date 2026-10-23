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

# Bind libc++ modules and headers to the selected compiler prefix. This avoids
# accidentally consuming a stale /usr/lib/llvm-XX installation when multiple
# Clang releases are present on the same host.
compiler="$(readlink -f "$(command -v clang++)")"
compiler_prefix="$(dirname "$(dirname "$compiler")")"
stdlib_module_dir="$compiler_prefix/share/libc++/v1"
stdlib_include_dir="$compiler_prefix/include/c++/v1"
if [[ ! -f "$stdlib_module_dir/std.cppm" ]]; then
    stdlib_module_dir=/usr/share/libc++/v1
fi
if [[ ! -f "$stdlib_include_dir/__config" ]]; then
    stdlib_include_dir=/usr/include/c++/v1
fi
[[ -f "$stdlib_module_dir/std.cppm" ]] || {
    echo "libc++ standard-library modules were not found for $compiler" >&2
    exit 1
}
[[ -f "$stdlib_include_dir/__config" ]] || {
    echo "libc++ headers were not found for $compiler" >&2
    exit 1
}
git -C "$root" submodule update --init --depth 1 -- 3rdparty/cnetmod

# Keep source builds deterministic without downloading protocol dependencies
# that cnetBoot does not enable (notably PostgreSQL's ICU and Raft's LevelDB).
dependencies=(
    3rdparty/boringssl
    3rdparty/json
    3rdparty/pugixml
    3rdparty/stdexec
    3rdparty/yaml-cpp
    3rdparty/yaml-cpp-modules
)
git -C "$cnetmod_source" submodule update --init --depth 1 -- "${dependencies[@]}"

required_files=(
    CMakeLists.txt
    3rdparty/boringssl/CMakeLists.txt
    3rdparty/json/include/nlohmann/json.hpp
    3rdparty/pugixml/CMakeLists.txt
    3rdparty/stdexec/include/exec/static_thread_pool.hpp
    3rdparty/yaml-cpp/CMakeLists.txt
    3rdparty/yaml-cpp-modules/CMakeLists.txt
)
for relative_path in "${required_files[@]}"; do
    [[ -f "$cnetmod_source/$relative_path" ]] || {
        echo "cnetmod dependency is incomplete: $relative_path" >&2
        exit 1
    }
done

fresh=()
if [[ "${CNETBOOT_FRESH:-0}" == 1 ]]; then fresh+=(--fresh); fi
cmake "${fresh[@]}" -S "$cnetmod_source" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(command -v clang)" \
    -DCMAKE_CXX_COMPILER="$(command -v clang++)" \
    -DCMAKE_ASM_COMPILER="$(command -v clang)" \
    -DSTDLIB_MODULE_DIRS="$stdlib_module_dir" \
    -DSTDLIB_INCLUDE_DIRS="$stdlib_include_dir" \
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
    -DCNETMOD_DEPENDENCY_MODE=system \
    -DCNETMOD_USE_SYSTEM_DEPS=OFF \
    -DCNETMOD_USE_MIMALLOC=OFF
cmake --build "$build_dir" --target cnetmod_core --parallel "$jobs"
cmake --install "$build_dir"

[[ -f "$prefix/lib/cmake/cnetmod/cnetmodConfig.cmake" ]] || {
    echo "cnetmod install is incomplete: $prefix" >&2
    exit 1
}
echo "WSL cnetmod installed at $prefix"
