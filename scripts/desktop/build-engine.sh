#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
target=${1:-linux}
shift || true
build_root=${CSVR_BUILD_ROOT:-"$project_dir/build-desktop"}
build_dir="$build_root/$target-engine"
cd "$project_dir/3rdparty/xash3d-fwgs"
case "$target" in
  linux)
    export CC=${CC:-clang} CXX=${CXX:-clang++}
    python3 waf configure -8 -T release --disable-vgui --disable-soft --disable-engine-mainui \
      --enable-bundled-deps --prefix=/usr -o "$build_dir" "$@"
    ;;
  windows)
    : "${CSVR_LLVM_MINGW:?Set CSVR_LLVM_MINGW to an extracted LLVM-MinGW toolchain}"
    : "${CSVR_WINDOWS_SDK:?Set CSVR_WINDOWS_SDK to a prefix containing SDL2 and OpenXR include/ and lib/}"
    export CC="$CSVR_LLVM_MINGW/bin/x86_64-w64-mingw32-clang"
    export CXX="$CSVR_LLVM_MINGW/bin/x86_64-w64-mingw32-clang++"
    export AR="$CSVR_LLVM_MINGW/bin/x86_64-w64-mingw32-ar"
    export WINRC="$CSVR_LLVM_MINGW/bin/x86_64-w64-mingw32-windres"
    python3 waf configure -8 -T release --disable-vgui --disable-soft --disable-engine-mainui \
      --enable-bundled-deps --prefix=/usr -s "$CSVR_WINDOWS_SDK" \
      --openxr-prefix="$CSVR_WINDOWS_SDK" -o "$build_dir" "$@"
    ;;
  *) echo "Usage: $0 linux|windows [Waf configure options]" >&2; exit 2 ;;
esac
python3 waf build -j "${CSVR_BUILD_JOBS:-8}"
