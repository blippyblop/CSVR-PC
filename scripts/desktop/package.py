#!/usr/bin/env python3
"""Package only verified distribution files; optionally include all pinned source."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', choices=['linux', 'windows'])
parser.add_argument('--source', action='store_true')
parser.add_argument('--output', type=Path, default=Path('build-desktop/releases'))
args = parser.parse_args()
if not args.target and not args.source:
    parser.error('Select --target and/or --source')
root = Path(__file__).resolve().parents[2]
version = (root/'VERSION').read_text().strip()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
prefix = f'CSVR-PC-{version}'
if args.target:
    stage = root/'stage-desktop'/f'{args.target}-x86_64'
    files = []
    for line in (stage/'SHA256SUMS').read_text().splitlines():
        expected, name = line.split('  ', 1)
        assert not Path(name).is_absolute() and '..' not in Path(name).parts
        assert hashlib.sha256((stage/name).read_bytes()).hexdigest() == expected, name
        files.append(name)
    files.append('SHA256SUMS')
    if args.target == 'linux':
        archive = output/f'{prefix}-linux-x86_64.tar.gz'
        with tarfile.open(archive, 'w:gz') as package:
            for name in files:
                package.add(stage/name, arcname=f'CSVR-PC/{name}', recursive=False)
    else:
        archive = output/f'{prefix}-windows-x86_64.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as package:
            for name in files:
                package.write(stage/name, arcname=f'CSVR-PC/{name}')
    print(archive)
if args.source:
    status = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root, text=True)
    assert not status, 'Commit all tracked source/submodule changes before packaging source'
    files = subprocess.check_output(['git', 'ls-files', '--recurse-submodules', '-z'], cwd=root).decode().split('\0')
    files = sorted(name for name in files if name and (root/name).is_file())
    assert not any(name.startswith(('game-data/', 'build-desktop/', 'stage-desktop/')) for name in files)
    with tarfile.open(output/f'{prefix}-source.tar.gz', 'w:gz') as package:
        for name in files:
            package.add(root/name, arcname=f'CSVR-PC-source/{name}', recursive=False)
    provenance = {'version': version,
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'submodules': subprocess.check_output(['git', 'submodule', 'status', '--recursive'], cwd=root, text=True).splitlines(),
        'files': {name: hashlib.sha256((root/name).read_bytes()).hexdigest() for name in files}}
    (output/'SOURCE_PROVENANCE.json').write_text(json.dumps(provenance, indent=2)+'\n')
    print(output/f'{prefix}-source.tar.gz')
(output/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n'
    for path in sorted(output.iterdir()) if path.is_file() and path.name != 'SHA256SUMS'))
