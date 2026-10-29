"""Crop actual 1080p renders and align the owner's reference at TV scale."""
import subprocess
from pathlib import Path

root = Path('/Volumes/ExternalSSD/tmp/rev/guialayout')
for state in ('before', 'after'):
    source = root / state / 'preview-playing-app.png'
    size = subprocess.check_output(['magick', 'identify', '-format', '%wx%h', str(source)], text=True)
    assert size == '1920x1080', size
    crop = '894x472+978+96' if state == 'after' else '455x256+1417+168'
    subprocess.run(['magick', str(source), '-crop', crop, '+repage', str(root / state / 'preview-frame.png')], check=True)
subprocess.run(['magick', str(root / 'after/pins-focused-app.png'), '-crop', '1824x96+48+984', '+repage', str(root / 'after/footer-focused.png')], check=True)
subprocess.run(['magick', '(', str(root / 'dono-guia-amarelo.jpg'), '-resize', '1920x1080!', ')', str(root / 'after/preview-playing-app.png'), '+append', str(root / 'owner-side-by-side.png')], check=True)
subprocess.run(['magick', str(root / 'before/preview-playing-app.png'), str(root / 'after/preview-playing-app.png'), '+append', str(root / 'before-after.png')], check=True)
print('PASS: 1080p renders, preview crops, footer focus and owner comparison')
