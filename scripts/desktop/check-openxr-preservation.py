#!/usr/bin/env python3
"""Verify the HMD baseline, allowing only recorded, reversible startup fixes."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root/'tests/desktop/openxr-preservation.json').read_text())
for name, expected in {**manifest['files'], **manifest['wire_files'], **manifest.get('startup_files', {})}.items():
    data = (root/name).read_bytes()
    change = manifest.get('reviewed_changes', {}).get(name)
    if change:
        assert hashlib.sha256(data).hexdigest() == change['sha256'], f'Reviewed fix changed: {name}'
        for replacement in reversed(change['replacements']):
            before, after = replacement['before'].encode(), replacement['after'].encode()
            assert after and data.count(after) == 1, f'Reviewed fix is missing or duplicated: {name}'
            data = data.replace(after, before, 1)
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise SystemExit(f'XR preservation check failed: {name}')
text = (root/'3rdparty/xash3d-fwgs/engine/client/cl_main.c').read_text()
start = text.index('void Host_ClientFrame( void )')
end = text.index('//============================================================================', start)
assert hashlib.sha256(text[start:end].encode()).hexdigest() == manifest['client_frame_sha256'], 'Per-eye client-frame handling changed'
changes = len(manifest.get('reviewed_changes', {}))
print(f'PASS: {len(manifest["files"])} HMD-baseline XR/UI/port files verified; {changes} reviewed startup file(s), all other bytes and per-eye frame handling unchanged')
print(f'PASS: {len(manifest.get("startup_files", {}))} additional startup source file(s) verified')
print(f'PASS: {len(manifest["wire_files"])} upstream protocol/serialization source files preserved byte-for-byte')
