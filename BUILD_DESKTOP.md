# Desktop OpenXR port — experimental

This checkout ports the Team Beef CSVR 1.0.5 source baseline to x86-64 Linux and
Windows using desktop OpenGL and OpenXR. The client, engine, renderer, filesystem,
bitmap-font menu, ReGameDLL server and YaPB library build in Release mode for
both targets. This is an initial port, not a verified playable release.

## Build and stage

Linux prerequisites: Clang, Python 3, CMake, Ninja, pkg-config, SDL2 development
files, OpenGL/EGL development files, OpenXR loader development files and zlib.

```sh
./scripts/desktop/build-engine.sh linux
./scripts/desktop/build-game.sh linux
ctest --test-dir build-desktop/linux-client --output-on-failure
python3 scripts/desktop/stage.py linux
```

Windows cross-build prerequisites: LLVM-MinGW and a compatible SDK prefix
containing SDL2 and OpenXR headers, import libraries and runtime DLLs.

```sh
export CSVR_LLVM_MINGW=/path/to/llvm-mingw
export CSVR_WINDOWS_SDK=/path/to/windows-sdk/mingw64
./scripts/desktop/build-engine.sh windows
./scripts/desktop/build-game.sh windows
python3 scripts/desktop/stage.py windows
```

Build engine targets sequentially: the vendored Waf source tree shares its
configuration lock. Game CMake build directories are independent. Artifacts are
staged under `stage-desktop/{linux,windows}-x86_64`. Nothing installs system files.
`SHA256SUMS` inventories only managed distribution files; game data and local
configs/screenshots in an existing staging directory are preserved and excluded.
Distribute the files in this manifest, not the entire used staging directory.

## Run

Provide owned Counter-Strike 1.6 `valve/` and `cstrike/` directories. Keep them in
a separate directory and pass `-rodir /path/to/game-root` to `csvr-xr` or
`csvr-desktop` (`.bat` on Windows). The launchers select `-vr` and `-novr`.
Linux defaults to native Wayland; explicit X11/GLX fallback is available with
`SDL_VIDEODRIVER=x11 SDL_VIDEO_X11_FORCE_EGL=0`. Select an installed OpenXR runtime
through the normal loader configuration or `XR_RUNTIME_JSON`.

The owned Steam installation used locally is ignored under `game-data/SteamCS16`.
It was obtained with the official SteamRE DepotDownloader and QR authentication;
no Steam credentials or proprietary game files belong in distribution artifacts.
Acquired baseline sources/APKs and provenance are stored separately in
`../artifacts/csvr-openxr-20261117/`. The beta APK has different native libraries;
this port does not claim to match its unpublished source revision.

For a paused de_dust2 scene with live head tracking and gameplay input suppressed
after pausing, run `./scripts/desktop/run-static.sh`. Set `CSVR_GAME_DATA` if the
owned data is elsewhere. Escape opens the game menu; the scene otherwise remains
running until closed. No desktop/display-manager service is restarted.

For an interactive team-menu test, run `./scripts/desktop/run-menu-check.sh`.
Either stick selects rows; the weapon-hand trigger confirms; B/Y or the menu
button goes back. In the main menu, horizontal stick input adjusts controls.
In-game team, class and buy menus use the server's valid numbered slots and
show a selection arrow. Confirming uses the existing `slotN` client command.
Controls held while closing a menu must be released before they reach gameplay.
Gameplay movement, turning, jumping and crouching retain their existing mappings.

The separate menu opened by clicking the left stick uses scripted touch buttons.
Run `./scripts/desktop/run-menu-check.sh --command` to test it. This menu now
captures controller navigation too: up/down selects rows, left/right selects
columns or ON/OFF cells, and the weapon-hand trigger activates the focused
button through its original click handler. New submenus receive focus and a
gold border identifies the current item. B/Y, the menu button, or another
left-stick click closes it. Hidden labels/backgrounds cannot receive focus.
The existing main-menu and numeric team/buy-menu input paths are preserved.
Console equivalents are `touch_menu_nav up|down|left|right`,
`touch_menu_activate`, and `touch_menu_back`.

## Stereo correction and diagnostics

The first HMD test was unusable: duplicated/displaced scene content, unintended
camera rotation and incorrect controls. OpenXR submission alone did not detect
these failures. Eye readback isolated a numeric bridge bug: `%g` emitted tiny
eye offsets in scientific notation, which Xash's `Q_atof` reads without the
exponent. Nanometre tracking noise consequently became metres of eye movement.
Pose values now use fixed decimal serialization.

Both eyes now share one client-view calculation and simulation snapshot. Each
render pass finishes before its swapchain image is released. Runtime eye poses
and asymmetric FOVs drive the camera; full quaternion composition supports eye
orientation relative to the tracking frame. A stable tracking-to-world heading
replaces camera feedback from render-pass CVars. Server teleports no longer
recenter/rotate the desktop XR reference space. Paused scenes continue to track
the head without accumulating movement/aim commands. Conventional SDL mouse and
gamepad look callbacks no longer compete with XR aim. Inactive actions are zeroed,
invalid controller poses lose their active flag, and the turn stick uses its
analog state with the configured deadzone. Touch, Index, WMR, Vive and simple
controller profiles are suggested; additional controller families remain untested.

The user confirmed correct world rendering, movement, turning, jump and crouch.
The remaining HUD and menu issues were traced to full-eye orthographic overlays,
Android touch-only menu scripts, and a pause filter suppressing menu input.
Desktop HUD drawing now projects a shared head-relative plane through each eye's
actual FOV, pose and relative quaternion. Defaults are 2 metres distance and
1.6 metres width (`vr_hud_distance`, `vr_hud_width`). The Android quarter-screen
sprite shrink is disabled on desktop, and spatial reticles are mapped back
through the HUD projection so their world aim remains correct. World camera
pose/projection handling is unchanged by this HUD fix.

Desktop selects classic menus and provides text equivalents for standard
VGUIMenu messages from servers that still send them. Menu input uses keyboard
navigation or valid numeric slots, with repeat timing and held-trigger latching,
instead of touch events. Menu input works while the game is paused. HUD fusion,
menu readability and physical menu-controller response were subsequently confirmed
working by the user in the HMD. Scaled bitmap text also uses logical glyph advances
to prevent overlapping characters on the XR HUD canvas.

`CSVR_XR_TRACE=1` enables bounded frame/eye/renderer diagnostics. To read back the
actual two OpenXR images before release, set `CSVR_XR_CAPTURE_PREFIX` to an
absolute output prefix and optionally `CSVR_XR_CAPTURE_FRAME` (default 600).
This produces `-left.ppm` and `-right.ppm` once; normal rendering has no readback.
Run `python3 scripts/desktop/check-stereo-log.py /path/to/trace.log` to compare
rendered origins against runtime IPD, check shared client views and eye order,
and reject XR/GL errors. Trace through at least frame 652 for this check.
The desktop mirror is off by default because a Wayland frame callback can block
XR submission. `CSVR_XR_MIRROR=1` opts in.

## Evidence and limits

Linux and Windows flat-desktop smoke tests loaded de_dust2, joined a team,
spawned, captured a screenshot and exited. Windows was exercised through Wine
using its native Wayland driver, not a native Windows OpenXR runtime.

Linux XR exercised the WiVRn runtime on an Intel Arc A770 and reached FOCUSED.
After the serialization fix, captures show one coherent world view per eye and
measured camera separation of approximately 65 mm at 30 game units/metre.
The matching logs and captures are under `build-desktop/{logs,evidence}`.
The user has confirmed world fusion/head tracking, basic gameplay controls, HUD
fusion, main/team menu navigation, and the separate left-stick command menu.
The latter also passed scripted root-to-submenu activation, focus/column/toggle
navigation and close tests; its controller navigation was then confirmed in HMD.
Motion weapon calibration, multiplayer,
bot integration and sustained performance still need HMD testing. Native Windows
OpenXR remains untested.

Three native tests exercise production frame/layer handling, real OpenGL FBOs,
and controller action polling plus pose serialization through the real engine
number parser. The frame test covers invalid tracking, nonrendering frames,
image ownership, resize deferral and teardown. The input test covers physical
hand mapping, grips/triggers, stick swap, invalid poses, inactive actions and
focus loss. Frame and input test executables also build for Windows and can run
under Wine; none of these tests establishes HMD comfort or full gameplay quality.
The GL test additionally checks shared HUD landmarks through asymmetric/canted
views against independent projected coordinates, plus inverse reticle mapping.
Input tests cover menu repeat timing, held-trigger latching and navigation across
sparse numeric slots. Real-runtime captures show the numeric team menu and a
continued correct world IPD without XR/GL errors.
