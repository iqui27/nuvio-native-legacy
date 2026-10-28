"""Compose native captures at identical scale with installed ImageMagick."""
from pathlib import Path
import subprocess
import sys
out = Path(sys.argv[1])
for name in ('my-lists', 'explore', 'trails', 'sources'):
    subprocess.run(['magick', str(out / f'{name}-before.png'),
                    str(out / f'{name}-after.png'), '+append',
                    str(out / f'{name}-comparison.png')], check=True)
ref = Path('/Volumes/ExternalSSD/tmp/rev/listas-ref-biblio.jpg')
if ref.exists():
    subprocess.run(['magick', '(', str(ref), '-resize', '1920x1080!', ')',
                    str(out / 'explore-after.png'), '+append',
                    str(out / 'library-reference-comparison.png')], check=True)
