# CSVR 1.0.6 integration and compatibility

Release: `1.0.6-pc.3`. Upstream tag: `v1.0.6`, commit
`6182611004cfff4377b81633435044a17031f46c`.

The PC port was checkpointed with its HMD-tested stereo, pose handling, HUD,
controller and scripted-menu fixes intact. Upstream 1.0.6 was merged on top;
the desktop engine tree was never replaced. The checkpoint remains in the
fork's history. Upstream 1.0.7 is deliberately outside this release.

The 1.0.6 engine change is `d2ed4a87c8f3d4fd7f014e1dbc11ec52aec3be9a`:
HTTP-backed static master-server lists, bounded in-memory HTTP responses,
protocol-specific server info requests and server-browser ping-base notification.
It changes six network/discovery files. Android version metadata advances to
1.0.6; this fork publishes PC packages only.

## Preserved code

`python3 scripts/desktop/check-openxr-preservation.py` verifies 48 files
against the validated desktop checkpoint: XR framebuffers,
lifecycle, eye transforms, stereo HUD, menu/controller handling, Windows bindings
and desktop client changes. It separately verifies per-eye `Host_ClientFrame`
handling in the network-modified `cl_main.c`.

PC.2 records a narrowly reviewed amendment in `VrBase.c`: desktop instance
creation requests OpenXR 1.0.0, rather than the SDK header's 1.1 version. The
desktop port uses 1.0 core plus explicitly enabled extensions; requiring 1.1
caused `XR_ERROR_API_VERSION_UNSUPPORTED` (-4) on Windows runtimes. Android keeps
its existing API request.

PC.3 fixes OpenGL startup validation in the same file: SDL's requested version
is replaced with the driver's actual `GL_VERSION`; OpenXR's tested maximum is
advisory, and the minimum remains mandatory, ignoring patch components as the
OpenXR OpenGL binding specifies. Diagnostics report actual/minimum/tested-maximum
versions. Windows XR requests an OpenGL 4.5 compatibility context through
`VrDesktopGL.h`, allowing fixed-function rendering on runtimes with a higher GL
minimum. Linux keeps its previous 3.3 compatibility request. Flat desktop
attributes are unchanged. `vid_sdl.c` only delegates its existing XR attribute
block to that helper; the rest of context and renderer lifecycle is unchanged.

The preservation check verifies both amended file hashes, reverses only the
recorded exact replacements in memory, then checks the original HMD baseline
hashes. It separately protects the new startup helper. All other baseline bytes,
including rendering and controller input, stay unchanged. Original reference
hashes have not been reset.

Eight protocol/network serializer files are also compared byte-for-byte with
upstream 1.0.5. Xash protocol 49 and GoldSrc/legacy protocol 48 constants and wire
serializers are unchanged. ReGameDLL gameplay source is unchanged; its PC branch
contains only build/platform portability fixes.

Reference hashes are in `tests/desktop/openxr-preservation.json` and checked in
PC CI before building. No XR fix was dropped to support older Linux dependencies:
the Ubuntu 22.04 build compiles the pinned SDL source and bundles that library.

## Validation scope

- The local HTTP fixture requests `/v1/servers/cstrike`, ignores unknown directives
  and emits exact protocol-49 and GoldSrc protocol-48 info packets.
- A 1.0.5 PC server accepts a 1.0.6 client; a 1.0.6 server accepts a 1.0.5
  client. Both complete signon, load de_dust2 and join a team.
- Linux servers accept Windows clients and Windows servers accept Linux clients
  with the same signon/team-join checks. Windows tests run under Wine.
- Production XR frame/input tests pass natively and as Windows executables under
  Wine. Real OpenGL tests check per-eye ownership and stereo HUD geometry through
  asymmetric/canted views.
- Production `VR_Init` is exercised against mocked 1.0-only and 1.1 runtimes on
  Linux and Windows/Wine. The 1.0-only test reproduces -4 before the PC.2 fix,
  then passes with it. This does not replace native Windows runtime/HMD testing.
- The same fixture reproduces PC.2's GL failure with requested 3.3/actual 4.6
  against a 4.5-minimum runtime. PC.3 accepts actual GL 4.5/4.6, preserves the
  compatibility profile, ignores version patch components and rejects GL 4.4,
  malformed versions and missing version-query functions with useful logs.
- A separate Windows executable creates the production compatibility-context
  attributes under Wine, verifies the actual profile/version and draws a
  fixed-function triangle with pixel readback. CI runs it after staging runtime
  DLLs. This exercises real OpenGL but not a native Windows headset/runtime.

These are loopback LAN tests with owned data. They do not certify public server
plugins, Steam/VAC authentication or all stock GoldSrc servers. Native Windows
OpenXR, sustained multiplayer, bots and weapon calibration need further testing.

## Repository structure

Patched submodule commits are published on `deps/xash3d-fwgs`, `deps/mainui_cpp`
and `deps/ReGameDLL_CS` in this same separate GitHub fork. Their upstream histories
and local port commits are preserved, without additional forks or local-only
gitlinks. Other dependency submodules retain upstream URLs.

Release assets include manifest-verified Linux/Windows builds, complete recursive
source, provenance, SHA-256 checksums and validation summaries. Owned game data
and authentication material are excluded.
