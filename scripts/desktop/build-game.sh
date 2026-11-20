#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
target=${1:-linux}
build_root=${CSVR_BUILD_ROOT:-"$project_dir/build-desktop"}
args=(-S "$project_dir" -B "$build_root/$target-client" -G Ninja
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
      -DBUILD_CLIENT=ON -DBUILD_MAINUI=ON -DBUILD_SERVER=ON
      -DMAINUI_USE_CUSTOM_FONT_RENDER=OFF -DBUILD_DESKTOP_XR_TESTS=ON)
case "$target" in
  linux) args+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++) ;;
  windows) : "${CSVR_LLVM_MINGW:?Set CSVR_LLVM_MINGW to an extracted LLVM-MinGW toolchain}"
           args+=(-DCMAKE_TOOLCHAIN_FILE="$project_dir/toolchains/windows-llvm-mingw.cmake") ;;
  *) echo "Usage: $0 linux|windows" >&2; exit 2 ;;
esac
cmake "${args[@]}"
cmake --build "$build_root/$target-client" --parallel "${CSVR_BUILD_JOBS:-8}"
