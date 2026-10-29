#!/usr/bin/env python3
"""Validate desktop screen coverage and report measured scroll timing."""
import json
from pathlib import Path
import re
import statistics
import struct
import subprocess

OUT = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
PROOF = OUT / 'polish'
SCREENS = ('home', 'detail', 'sidebar-expanded', 'sidebar-collapsed', 'streaming', 'search-typing',
           'library', 'lists', 'filmography', 'filmography-scrolled', 'settings', 'guide',
           'sports', 'achievements', 'player-overlay')
images = []
for width, height in ((1280, 800), (1920, 1080)):
    for screen in SCREENS:
        path = PROOF / f'{screen}-{width}x{height}.png'
        data = path.read_bytes()
        assert data.startswith(b'\x89PNG\r\n\x1a\n'), path
        assert struct.unpack('>II', data[16:24]) == (width, height), path
        assert len(data) > 10000, path
        images.append(str(path))
for appearance in ('Aqua', 'DarkAqua'):
    path = PROOF / f'native-{appearance}.png'
    assert path.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
    images.append(str(path))
log = (PROOF / 'runtime.log').read_text(errors='replace')
assert '[desktop-native]' in log
assert 'material=Sidebar transparent=1' in log
samples = [tuple(map(int, match)) for match in re.findall(
    r'desktop-scroll\] t=(\d+) work_ms=(\d+) wait_ms=(\d+)', log)]
assert len(samples) >= 30, len(samples)
work = sorted(sample[1] for sample in samples)
intervals = sorted(b[0]-a[0] for a,b in zip(samples,samples[1:]) if 0 < b[0]-a[0] < 100)
report = {'frames': len(samples), 'work_median_ms': statistics.median(work),
          'work_p95_ms': work[int(len(work)*.95)], 'work_max_ms': max(work),
          'frame_interval_median_ms': statistics.median(intervals),
          'input': 'Synthetic native-pixel scroll events, production pointer route; no physical trackpad measurement',
          'pngs': images}
(PROOF / 'proof.json').write_text(json.dumps(report, indent=2) + '\n')
(PROOF / 'frame-times.log').write_text('\n'.join(line for line in log.splitlines() if '[desktop-scroll]' in line) + '\n')
print(json.dumps({key:value for key,value in report.items() if key != 'pngs'}, indent=2))
