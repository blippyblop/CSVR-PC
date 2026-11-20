# CSVR 1.0.6 integration and compatibility

Release: `1.0.6-pc.1`. Upstream tag: `v1.0.6`, commit
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

`python3 scripts/desktop/check-openxr-preservation.py` verifies byte-identical
content for 48 files from the validated desktop checkpoint: XR framebuffers,
lifecycle, eye transforms, stereo HUD, menu/controller handling, Windows bindings
and desktop client changes. It separately verifies per-eye `Host_ClientFrame`
handling in the network-modified `cl_main.c`.

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
