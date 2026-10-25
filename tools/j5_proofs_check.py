#!/usr/bin/env python3
"""Reject stale J5 native sources or a trio assembled from different pixels."""
import hashlib
import json
import subprocess
from pathlib import Path

root = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/redesign/j5')
manifest = json.loads((root / 'proofs.json').read_text())
assert manifest['clock_times_ms'] == [0, 50, 100, 150, 250, 400]
current = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
assert manifest['source_commit'] == current
code_time = Path('src/juntosui.c').stat().st_mtime_ns
sources = {item['path']: item for item in manifest['native_sources']}
for name, item in sources.items():
    path = Path(name)
    assert path.resolve().is_relative_to(root), path
    assert path.stat().st_mtime_ns == item['mtime_ns'], path
    assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], path
    if path.parent.name == 'together-verified':
        assert path.stat().st_mtime_ns > code_time, path
for name in manifest['pngs']:
    assert Path(name).stat().st_size > 0, name

def pixels(path, crop=None):
    args = ['magick', str(path)]
    if crop:
        args += ['-crop', crop, '+repage']
    return hashlib.sha256(subprocess.check_output(args + ['-depth', '8', 'rgba:-'])).digest()

checked = 0
for screen, folder, pattern in (
    ('home21', 'home21', 'home21'),
    ('servers-detail', 'servers-detail', 'server'),
    ('servers-sources', 'servers-sources', 'native'),
    ('servers-overview', 'servers-overview', 'native-overview'),
    ('together', 'together-verified', 'matrix-3'),
):
    if not (root / folder).exists():
        continue
    checked += 4
    for scale in (100, 85):
        for material in ('glass', 'solid'):
            trio = root / f'{screen}-{scale}-{material}-tri.png'
            native = root / folder / f'{pattern}-{scale}-{material}.png'
            assert pixels(trio, '1920x1080+3840+0') == pixels(native), trio
print(f'PASS: {len(sources)} fresh native sources; {checked} exact app columns; current commit')
