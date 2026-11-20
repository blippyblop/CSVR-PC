#!/usr/bin/env python3
"""Fetch the checksum-pinned LLVM-MinGW/SDL2/OpenXR SDK used by the PC release."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=Path('build-desktop/windows-deps'))
parser.add_argument('--cache', type=Path, default=Path('build-desktop/downloads'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
args.cache.mkdir(parents=True, exist_ok=True)
lock = json.loads(Path(__file__).with_name('windows-dependencies.json').read_text())
for dependency in [lock['toolchain']] + lock['sdk']:
    archive = args.cache/dependency['url'].rsplit('/', 1)[1]
    if not archive.exists():
        temporary = archive.with_suffix(archive.suffix+'.part')
        with urllib.request.urlopen(dependency['url'], timeout=120) as source, temporary.open('wb') as destination:
            while data := source.read(1024*1024):
                destination.write(data)
        temporary.replace(archive)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != dependency['sha256']:
        raise SystemExit(f'Checksum mismatch: {archive.name}')
    destination = args.output if dependency is lock['toolchain'] else args.output/'sdk'
    destination.mkdir(exist_ok=True)
    subprocess.run(['tar', '-xf', str(archive), '-C', str(destination),
                    '--exclude=.PKGINFO', '--exclude=.BUILDINFO', '--exclude=.MTREE'], check=True)
toolchain = (args.output/lock['toolchain']['name']).resolve()
sdk = (args.output/'sdk/mingw64').resolve()
assert (toolchain/'bin/x86_64-w64-mingw32-clang').exists()
assert (sdk/'bin/SDL2.dll').exists() and (sdk/'bin/libopenxr_loader.dll').exists()
environment = args.output/'env.sh'
environment.write_text(f'export CSVR_LLVM_MINGW={shlex.quote(str(toolchain))}\n'
                       f'export CSVR_WINDOWS_SDK={shlex.quote(str(sdk))}\n')
print(f'Verified Windows dependencies. Source {environment} before building.')
