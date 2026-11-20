#!/usr/bin/env python3
"""Check actual game-renderer traces against runtime stereo geometry."""
import argparse
import math
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('--world-scale', type=float, default=30)
args = parser.parse_args()
text = args.log.read_text(errors='replace')
assert 'XR_ERROR_' not in text and not re.search(r'OpenGL\s+Error', text, re.I), 'Runtime/GL error in trace'
vector = r'\(([-+\d.]+) ([-+\d.]+) ([-+\d.]+)\)'
runtime = re.findall(r'XR geometry frame=(\d+) eye=([01]) pose=' + vector, text)
ipds = []
for a, b in zip(runtime[::2], runtime[1::2]):
    assert a[0] == b[0] and a[1] == '0' and b[1] == '1', 'Unpaired located views'
    ipds.append(math.dist(tuple(map(float, a[2:])), tuple(map(float, b[2:]))))
assert ipds, 'No located-eye geometry; enable CSVR_XR_TRACE and run at least 652 frames'
expected = sum(ipds) / len(ipds)
assert 0.03 < expected < 0.10, f'Implausible runtime IPD: {expected}'
render = re.findall(r'XR renderer eye=([01]) base=' + vector + r' org=' + vector + r' angles=' + vector, text)
assert len(render) >= 10 and len(render) % 2 == 0, 'Need at least five complete rendered eye pairs'
measured = []
for a, b in zip(render[::2], render[1::2]):
    assert a[0] == '0' and b[0] == '1', 'Eye render order mismatch'
    assert a[1:4] == b[1:4], 'Eyes use different client-view snapshots'
    left, right = tuple(map(float, a[4:7])), tuple(map(float, b[4:7]))
    separation = math.dist(left, right) / args.world_scale
    assert abs(separation - expected) < 0.0005, f'Rendered IPD {separation:.6f}m differs from runtime {expected:.6f}m'
    pitch, yaw, roll = map(math.radians, map(float, a[7:10]))
    sp, sy, sr = math.sin(pitch), math.sin(yaw), math.sin(roll)
    cp, cy, cr = math.cos(pitch), math.cos(yaw), math.cos(roll)
    camera_right = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    eye_delta = tuple(r - l for l, r in zip(left, right))
    alignment = sum(x * y for x, y in zip(camera_right, eye_delta)) / math.dist(left, right)
    assert alignment > 0.9, f'Eye ordering/depth is reversed or inconsistent: {alignment:.3f}'
    measured.append(separation * 1000)
print(f'PASS: {len(measured)} shared-view eye pairs, runtime IPD {expected * 1000:.2f} mm, rendered {min(measured):.2f}–{max(measured):.2f} mm, correct eye order, no XR/GL errors')
