#!/usr/bin/env python3
"""Stage desktop builds locally. No installation and no game-data download."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('target', choices=['linux', 'windows'])
args = parser.parse_args()
project = Path(__file__).resolve().parents[2]
target = args.target
build_root = Path(os.environ.get('CSVR_BUILD_ROOT', str(project / 'build-desktop')))
engine = build_root / (target + '-engine')
game = build_root / (target + '-client')
stage = project / 'stage-desktop' / (target + '-x86_64')
stage.mkdir(parents=True, exist_ok=True)
managed = set()

def copy(source, relative):
    source = Path(source)
    if not source.is_file():
        raise SystemExit(f'Missing build artifact: {source}')
    destination = stage / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Replace complete files atomically; never truncate an ELF/DLL still mapped
    # by a test process that is exiting or using the previous staged build.
    fd, temporary = tempfile.mkstemp(prefix='.stage-', dir=destination.parent)
    os.close(fd)
    try:
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    managed.add(str(relative))

suffix = 'dll' if target == 'windows' else 'so'
copy(engine / 'engine' / ('xash.dll' if target == 'windows' else 'libxash.so'), 'xash.dll' if target == 'windows' else 'libxash.so')
copy(engine / 'game_launch' / ('xash3d.exe' if target == 'windows' else 'xash3d'), 'xash3d.exe' if target == 'windows' else 'xash3d')
copy(engine / 'filesystem' / ('filesystem_stdio.' + suffix), 'filesystem_stdio.' + suffix)
copy(engine / 'ref/gl' / ('ref_gl.dll' if target == 'windows' else 'libref_gl.so'), 'ref_gl.dll' if target == 'windows' else 'libref_gl.so')
for name in ['client', 'menu']:
    source = game / ('cl_dll' if name == 'client' else '3rdparty/mainui_cpp') / f'{name}_amd64.{suffix}'
    copy(source, f'cstrike/cl_dlls/{name}_amd64.{suffix}')
server = 'mp' if target == 'windows' else 'cs'
copy(game / '3rdparty/ReGameDLL_CS/regamedll' / f'{server}_amd64.{suffix}', f'cstrike/dlls/{server}_amd64.{suffix}')
copy(game / '3rdparty/yapb' / f'yapb_amd64.{suffix}', f'cstrike/dlls/yapb_amd64.{suffix}')
copy(game / 'extras.pk3', 'cstrike/extras.pk3')
copy(engine / '3rdparty/extras/extras.pk3', 'valve/extras.pk3')
copy(project / 'LICENSE', 'LICENSE-CSVR.txt')
copy(project / 'VERSION', 'VERSION')
copy(project / 'THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES.md')

if target == 'windows':
    sdk = Path(os.environ['CSVR_WINDOWS_SDK'])
    compiler = Path(os.environ['CSVR_LLVM_MINGW'])
    copy(sdk / 'bin/SDL2.dll', 'SDL2.dll')
    copy(sdk / 'bin/libopenxr_loader.dll', 'libopenxr_loader.dll')
    for component in ['SDL2', 'openxr-sdk', 'libgcc', 'libstdc++', 'libwinpthread']:
        directory = sdk / 'share/licenses' / component
        if not directory.is_dir():
            raise SystemExit(f'Missing runtime license directory: {directory}')
        for source in sorted(directory.rglob('*')):
            if source.is_file():
                copy(source, str(Path('licenses') / component / source.relative_to(directory)))
    copy(compiler / 'LICENSE.TXT', 'licenses/LLVM-MinGW-LICENSE.TXT')
    # Resolve third-party dependencies recursively. System DLLs remain provided
    # by Windows. Match case-insensitively, as the Windows loader does.
    available = {}
    for directory in [sdk / 'bin', compiler / 'x86_64-w64-mingw32/bin']:
        for path in directory.glob('*.dll'):
            available[path.name.lower()] = path
    pending = [stage / name for name in managed if name.endswith(('.dll', '.exe'))]
    seen = set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        imports = subprocess.check_output(['objdump', '-p', str(binary)], text=True)
        for name in re.findall(r'DLL Name:\s*(\S+)', imports):
            source = available.get(name.lower())
            if source and source.name not in managed:
                copy(source, source.name)
                pending.append(stage / source.name)
    (stage / 'csvr-xr.bat').write_text('@echo off\ncd /d "%~dp0"\nxash3d.exe -game cstrike -ref gl -windowed -vr %*\n', newline='\r\n')
    (stage / 'csvr-desktop.bat').write_text('@echo off\ncd /d "%~dp0"\nxash3d.exe -game cstrike -ref gl -windowed -novr %*\n', newline='\r\n')
    managed.update(['csvr-xr.bat', 'csvr-desktop.bat'])
else:
    if os.environ.get('CSVR_LINUX_SDK'):
        sdk = Path(os.environ['CSVR_LINUX_SDK'])
        copy(sdk / 'lib/libSDL2-2.0.so.0', 'libSDL2-2.0.so.0')
        copy(project / '3rdparty/SDL/LICENSE.txt', 'licenses/SDL2-LICENSE.txt')
    for name, mode in [('csvr-xr', '-vr'), ('csvr-desktop', '-novr')]:
        script = stage / name
        script.write_text('#!/usr/bin/env bash\nset -euo pipefail\ncd -- "$(dirname -- "${BASH_SOURCE[0]}")"\n'
                          'export LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
                          'export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"\n'
                          f'exec ./xash3d -game cstrike -ref gl -windowed {mode} "$@"\n')
        script.chmod(0o755)
        managed.add(name)

(stage / 'README.txt').write_text('CSVR-PC ' + (project / 'VERSION').read_text().strip() + ' — experimental desktop OpenXR fork.\n'
    'Provide your owned Counter-Strike 1.6 valve/ and cstrike/ game folders.\n'
    'You can keep them read-only and pass -rodir /path/to/game-root to a launcher.\n'
    'Linux defaults to Wayland. Explicit X11/GLX fallback: SDL_VIDEODRIVER=x11 SDL_VIDEO_X11_FORCE_EGL=0 ./csvr-xr\n'
    'Windows: csvr-xr.bat. Desktop mode: csvr-desktop[.bat].\n'
    'Experimental port: builds and frame/FBO/input tests pass.\n'
    'World stereo/head tracking and basic gameplay controls were confirmed in HMD.\n'
    'HUD now uses a shared stereo plane; menus use stick navigation and trigger\n'
    'confirmation; both were confirmed working in HMD. Native Windows XR is untested.\n'
    'LAN signon/team join is checked across versions/platforms; Internet authentication is not certified.\n'
    'The YaPB library is included; full bot integration needs gameplay testing.\n'
    'See BUILD_DESKTOP.md in the source checkout for build and validation details.\n')
managed.add('README.txt')
copy(project / 'BUILD_DESKTOP.md', 'BUILD_DESKTOP.md')
files = {name: hashlib.sha256((stage / name).read_bytes()).hexdigest() for name in sorted(managed)}
(stage / 'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name, digest in files.items()))
print(json.dumps({'stage': str(stage), 'files': len(files)}, indent=2))
