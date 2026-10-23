#!/usr/bin/env python3
"""Check native PNG output and create reviewable mock | app comparisons."""
import pathlib, struct, subprocess, sys, zlib
root = pathlib.Path(sys.argv[1])
mock = pathlib.Path('/Volumes/ExternalSSD/tmp/servidores-polish')
files = sorted(root.glob('*-app.png'))
assert files, 'No app captures'
for app in files:
    b = app.read_bytes()
    assert b[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', b[16:24]) == (1920, 1080)
    data = bytearray(); i = 8
    while i < len(b):
        n = struct.unpack('>I', b[i:i+4])[0]
        if b[i+4:i+8] == b'IDAT': data.extend(b[i+8:i+8+n])
        i += n+12
    raw = zlib.decompress(data)
    assert len(set(raw)) > 32 and len(b) > 15000, f'Blank capture: {app}'
    ref = mock / app.name.replace('-app.png', '.png')
    if ref.exists():
        out = root / app.name.replace('-app.png', '-side-by-side.png')
        subprocess.run(['magick', str(ref), str(app), '+append', str(out)], check=True)
print(f'{len(files)} nonblank 1920x1080 app captures checked')
