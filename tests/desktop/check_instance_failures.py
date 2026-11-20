#!/usr/bin/env python3
"""Check that production VR_Init rejects invalid/insufficient GL with useful logs."""
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wine', action='store_true')
parser.add_argument('executable')
args = parser.parse_args()
prefix = ['wine'] if args.wine else []
for scenario, expected in [
    ('below', 'OpenXR needs OpenGL 4.5 or newer; the current context is 4.4.'),
    ('malformed', 'Cannot read the current desktop OpenGL version (GL_VERSION=not a GL version).'),
    ('missing', 'Cannot read the current desktop OpenGL version (GL_VERSION=unavailable).'),
]:
    result = subprocess.run(prefix + [args.executable, '0', scenario],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=30)
    assert result.returncode == 1 and expected in result.stdout, (scenario, result.returncode, result.stdout)
    assert 'PASS:' not in result.stdout, result.stdout
    print(f'PASS: production VR_Init rejects {scenario} GL context with expected diagnostic')
