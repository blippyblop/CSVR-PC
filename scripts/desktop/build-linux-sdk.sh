#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_root=${CSVR_BUILD_ROOT:-"$project_dir/build-desktop"}
sdk=${CSVR_LINUX_SDK:-"$build_root/linux-sdk"}
# Upstream's SDL APIs require >=2.24. Use the pinned SDL submodule so the
# Ubuntu 22.04 release need not change any HMD-validated engine source.
cmake -S "$project_dir/3rdparty/SDL" -B "$build_root/linux-sdl" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$sdk" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    -DSDL_WAYLAND=ON -DSDL_X11=ON
cmake --build "$build_root/linux-sdl" --parallel "${CSVR_BUILD_JOBS:-8}"
cmake --install "$build_root/linux-sdl"
