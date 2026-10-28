"""Compose native Sports before/after proof at equal scale."""
from pathlib import Path
import subprocess

out = Path('/Volumes/ExternalSSD/tmp/rev/sportsux')
for state in ('empty', 'home'):
    subprocess.run(['magick', str(out / f'before-{state}-app.png'),
                    str(out / f'after-{state}-app.png'), '+append',
                    str(out / f'{state}-compare.png')], check=True)
print('PASS Sports before/after PNG comparisons')
