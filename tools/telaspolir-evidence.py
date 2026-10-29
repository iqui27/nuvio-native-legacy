#!/usr/bin/env python3
"""Create TV review JPEG pairs and a contact sheet using installed ImageMagick."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/Volumes/ExternalSSD/tmp/rev/telaspolir')
after = sorted((root / 'after').glob('*.png'))
assert after, 'No native after captures'
pairs = root / 'comparisons'
pairs.mkdir(exist_ok=True)
matched, missing = [], []
for path in after:
    before = root / 'before' / path.name
    for image in (path, before):
        if not image.exists():
            continue
        size = subprocess.check_output(['magick', 'identify', '-format', '%wx%h', str(image)], text=True)
        assert size == '1920x1080', (image, size)
        subprocess.run(['magick', str(image), '-quality', '91', str(image.with_suffix('.jpg'))], check=True)
    if not before.exists():
        missing.append(path.name)
        continue
    subprocess.run(['magick', str(before), str(path), '+append',
                    '-quality', '91', str(pairs / f'{path.stem}.jpg')], check=True)
    matched.append(path.name)
subprocess.run(['magick', 'montage', *map(str, after), '-set', 'label', '%t', '-label', '%t',
                '-font', '/System/Library/Fonts/Supplemental/Arial.ttf', '-pointsize', '18', '-fill', '#f3f2ef', '-background', '#121316',
                '-tile', '4x', '-geometry', '480x270+0+12', str(root / 'CONTACT-SHEET.png')], check=True)
subprocess.run(['magick', str(root / 'CONTACT-SHEET.png'), '-quality', '92', str(root / 'CONTACT-SHEET.jpg')], check=True)
manifest = {'tv_after': len(after), 'paired': len(matched), 'missing_before': missing,
            'pngs': [str(p.resolve()) for p in after], 'contact_sheet': str(root / 'CONTACT-SHEET.png')}
(root / 'evidence.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'TV after: {len(after)}, paired: {len(matched)}, missing before: {len(missing)}')
raise SystemExit(1 if missing else 0)
