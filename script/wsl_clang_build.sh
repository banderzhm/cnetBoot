#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$root/backend"
build_dir="${CNETBOOT_BUILD_DIR:-$source_dir/build-clang}"
cnetmod_dir="$root/3rdparty/wsl-clang/lib/cmake/cnetmod"
jobs="${CNETBOOT_JOBS:-2}"
build_testing="${CNETBOOT_BUILD_TESTING:-OFF}"

for tool in cmake ninja clang++; do
    command -v "$tool" >/dev/null || { echo "Missing required tool: $tool" >&2; exit 1; }
done
[[ -f "$cnetmod_dir/cnetmodConfig.cmake" ]] || {
    echo "cnetmod is not installed. Run ./script/wsl_clang_3rdparty_build.sh first." >&2
    echo "Missing: $cnetmod_dir/cnetmodConfig.cmake" >&2
    exit 1
}
cnetmod_prefix="$(cd "$cnetmod_dir/../../.." && pwd)"

fresh=()
if [[ "${CNETBOOT_FRESH:-0}" == 1 ]]; then fresh+=(--fresh); fi
cmake "${fresh[@]}" -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$(command -v clang++)" \
    -DBUILD_TESTING="$build_testing" \
    -DCMAKE_PREFIX_PATH="$cnetmod_prefix" \
    -Dcnetmod_DIR="$cnetmod_dir"
cmake --build "$build_dir" --parallel "$jobs"
