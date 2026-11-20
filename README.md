# CSVR-PC

Independent **Linux and Windows x86-64 OpenXR** fork of [Team Beef CSVR 1.0.6](https://github.com/Team-Beef-Studios/CSVR/releases/tag/v1.0.6). Includes the client, Xash3D engine, OpenGL renderer, bitmap-font menu, ReGameDLL server and YaPB library. Flat desktop mode is included.

This is an experimental PC port, separate from the official Quest/Pico release. Linux world rendering, stereo HUD, movement/turning/jump/crouch, main/team menus and the left-stick command menu have been confirmed working in an HMD. Windows builds and LAN interoperability are tested through Wine; native Windows OpenXR remains untested.

## Download and run

Get PC builds and complete source from [Releases](https://github.com/jebot-git/CSVR-PC/releases). Extract into a writable folder. Supply owned Counter-Strike 1.6 `valve/` and `cstrike/` directories; Steam game data is not included.

Linux (Wayland by default):

```sh
./csvr-xr -rodir /path/to/your/Counter-Strike
# Flat desktop mode:
./csvr-desktop -rodir /path/to/your/Counter-Strike
```

Windows:

```bat
csvr-xr.bat -rodir "C:\path\to\Counter-Strike"
rem Flat desktop mode:
csvr-desktop.bat -rodir "C:\path\to\Counter-Strike"
```

An installed OpenXR runtime and graphics driver are required for VR. Linux release binaries are built on Ubuntu 22.04 and include SDL2; provide the system OpenXR loader, OpenGL/EGL, X11 client library and C/C++ runtimes. X11/GLX is an explicit fallback: `SDL_VIDEODRIVER=x11 SDL_VIDEO_X11_FORCE_EGL=0`.

## Compatibility and upstream fixes

The 1.0.6 FWGS HTTP master-server discovery fix is merged without changing game protocol numbers or wire serializers. Tests cover old/new PC client-server signon in both directions and Linux/Windows LAN connections. The master fixture verifies both Xash protocol-49 and GoldSrc protocol-48 discovery packets. Internet authentication, anti-cheat services and every public server/mod have not been certified.

All HMD-tested OpenXR/HUD/controller fixes are retained. A preservation check protects 48 XR/UI/port files, per-eye frame handling and eight network protocol/serialization files. See [upstream integration](docs/UPSTREAM_INTEGRATION.md).

PC.2 fixes Windows startup error `-4` (`XR_ERROR_API_VERSION_UNSUPPORTED`) by requesting OpenXR 1.0 core instead of the newer SDK header version. Rendering, HUD and controller code are unchanged.

## Build

```sh
git clone --recursive https://github.com/jebot-git/CSVR-PC.git
cd CSVR-PC
python3 scripts/desktop/check-openxr-preservation.py
./scripts/desktop/build-engine.sh linux
./scripts/desktop/build-game.sh linux
ctest --test-dir build-desktop/linux-client --output-on-failure
python3 scripts/desktop/stage.py linux
```

Patched engine/menu/server histories are pinned submodules hosted on `deps/*` branches **in this same fork**. Other dependencies retain upstream URLs. The complete source release includes all recursive submodules.

[Build instructions, controls, evidence and limitations](BUILD_DESKTOP.md) · [Third-party notices](THIRD_PARTY_NOTICES.md) · [Original upstream README and credits](docs/UPSTREAM_README.md)
