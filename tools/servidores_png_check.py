#!/usr/bin/env python3
"""Check native PNG output and create reviewable mock | app comparisons."""
import itertools, json, pathlib, struct, subprocess, sys, zlib
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
    if app.name == 'detalhe-chip-app.png':
        proof = json.loads((root / 'detalhe-chip-rects.json').read_text())
        rects = proof['rects']
        assert len(rects) == 5, f'Expected friends, Vai tocar, two servers and caption: {rects}'
        overlaps = []
        for a, b in itertools.combinations(rects, 2):
            if (max(a[0], b[0]) < min(a[0]+a[2], b[0]+b[2]) and
                    max(a[1], b[1]) < min(a[1]+a[3], b[1]+b[3])):
                overlaps.append((a, b))
        assert not overlaps, f'Overlapping detail rectangles: {overlaps}'
        assert all(w > 0 and h > 0 and x >= 0 and y >= 0 and
                   x+w <= 1920 and y+h <= 1080 for x, y, w, h in rects)
        assert proof['navigation_ok'], 'D-pad must follow actions, servers, friends, sections in both directions'
        print('PASS: five detail rectangles do not overlap; D-pad follows visual order')
    ref = mock / app.name.replace('-app.png', '.png')
    if ref.exists():
        out = root / app.name.replace('-app.png', '-side-by-side.png')
        subprocess.run(['magick', str(ref), str(app), '+append', str(out)], check=True)
print(f'{len(files)} nonblank 1920x1080 app captures checked')
