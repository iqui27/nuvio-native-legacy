#!/usr/bin/env python3
"""Build labeled, full-resolution before/after evidence from shot-test PNGs."""
from pathlib import Path
import subprocess

root = Path('/Volumes/ExternalSSD/tmp/rev/epsmoderno')
for before in sorted((root / 'before').glob('*.png')):
    after = root / 'after' / before.name
    output = root / f'compare-{before.name}'
    subprocess.run(['magick', str(before), str(after), '+append',
                    '-background', '#121316', '-gravity', 'north', '-splice', '0x40',
                    '-font', 'deploy/app/fonts/InterDisplay-Regular.ttf', '-pointsize', '20', '-fill', 'white',
                    '-gravity', 'northwest', '-annotate', '+24+8', f'BEFORE | {before.stem}',
                    '-annotate', '+1944+8', f'AFTER | {before.stem}', str(output)], check=True)
    print(output)
assert len(list(root.glob('compare-*.png'))) == 10
print('PASS: ten full-resolution before/after comparisons')
