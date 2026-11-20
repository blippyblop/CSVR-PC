# Third-party components

CSVR-PC is an independent, noncommercial desktop fork of Team Beef CSVR 1.0.6,
which builds on CS16Client and Xash3D FWGS. Copyright notices and source-file
license headers remain intact. The complete recursively pinned source is
provided as a separate release asset.

Application components include Team Beef CSVR/CS16Client, Xash3D FWGS, mainui_cpp,
ReGameDLL_CS, YaPB, MiniUTL and their pinned dependencies. Their license files and
notices are included in the corresponding source directories; see the root
`LICENSE` and individual file headers for applicable terms.

The portable Linux build bundles SDL2, built from the pinned SDL source submodule
under the zlib license. Its license is included in `licenses/SDL2-LICENSE.txt`.
OpenXR, OpenGL/EGL and the C/C++ runtimes are supplied by the Linux system.

The Windows build bundles SDL2 (zlib), the Khronos OpenXR loader
(Apache-2.0/MIT), LLVM-MinGW runtime libraries (LLVM and associated runtime
licenses), GCC runtime libraries (GPL with the GCC Runtime Library Exception),
and winpthreads. The corresponding notices are included under `licenses/`.
Exact dependency URLs and SHA-256 hashes are pinned in
`scripts/desktop/windows-dependencies.json` in the source release.

Upstream sources:

- https://github.com/Team-Beef-Studios/CSVR
- https://github.com/Team-Beef-Studios/xash3d-fwgs
- https://github.com/Team-Beef-Studios/mainui_cpp
- https://github.com/Team-Beef-Studios/ReGameDLL_CS
- https://github.com/yapb/yapb
- https://github.com/libsdl-org/SDL
- https://github.com/KhronosGroup/OpenXR-SDK
- https://github.com/mstorsjo/llvm-mingw
- https://www.msys2.org/

Valve Counter-Strike and Half-Life game data, Steam credentials and Valve
executables are not included. Supply your own licensed `valve/` and `cstrike/`
data directories.
