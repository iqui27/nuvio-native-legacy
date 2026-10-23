#!/usr/bin/env python3
"""Compare Current against pre-change hero pixels; baseline is evidence, not a fixture."""
import pathlib
import struct
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1])

def rgb(path):
    with tempfile.TemporaryDirectory() as directory:
        bmp = pathlib.Path(directory) / 'image.bmp'
        subprocess.run(['sips', '-s', 'format', 'bmp', str(path), '--out', str(bmp)],
                       check=True, stdout=subprocess.DEVNULL)
        data = bmp.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert w == 1920 and abs(h) == 1080 and bits in (24, 32)
    step = bits // 8
    stride = (w * step + 3) & ~3
    return bytes(data[offset + (abs(h) - 1 - y if h > 0 else y) * stride + x * step + c]
                 for y in range(110, 480) for x in range(100, 1900) for c in range(3))

for scale in (85, 100):
    for glass in (0, 1):
        before = root / f'before-{scale}-g{glass}-m0-stage.png'
        after = root / f'after-{scale}-g{glass}-m1-stage.png'
        if not before.exists():
            print(f'Baseline unavailable: {before}; draw assertions still cover Current')
            continue
        a, b = rgb(before), rgb(after)
        changed = sum(x != y for x, y in zip(a, b))
        peak = max(abs(x - y) for x, y in zip(a, b))
        print(f'Current baseline {scale}% glass={glass}: changed channels={changed}, max delta={peak}')
        assert changed == 0, 'Current hero differs from baseline'
