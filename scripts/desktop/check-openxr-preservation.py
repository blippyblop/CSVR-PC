#!/usr/bin/env python3
"""Verify that upstream integration preserves the HMD-validated XR code exactly."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root/'tests/desktop/openxr-preservation.json').read_text())
for name, expected in {**manifest['files'], **manifest['wire_files']}.items():
    actual = hashlib.sha256((root/name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f'XR preservation check failed: {name}')
text = (root/'3rdparty/xash3d-fwgs/engine/client/cl_main.c').read_text()
start = text.index('void Host_ClientFrame( void )')
end = text.index('//============================================================================', start)
assert hashlib.sha256(text[start:end].encode()).hexdigest() == manifest['client_frame_sha256'], 'Per-eye client-frame handling changed'
print(f'PASS: {len(manifest["files"])} HMD-validated XR/UI/port source files and per-eye client-frame handling preserved byte-for-byte')
print(f'PASS: {len(manifest["wire_files"])} upstream protocol/serialization source files preserved byte-for-byte')
