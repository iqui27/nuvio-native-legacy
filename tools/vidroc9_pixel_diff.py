#!/usr/bin/env python3
"""Compare C9 cache shots, masking only the wall-clock island digits."""
import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path('/Volumes/ExternalSSD/tmp/rev/vidroc9-shots')
with tempfile.TemporaryDirectory() as work:
    for screen in ('home', 'detail', 'settings'):
        masked = [str(pathlib.Path(work) / f'{i}.png') for i in range(2)]
        for stage, target in zip(('before', 'after'), masked):
            subprocess.run(['magick', str(root / stage / f'{screen}.png'),
                            '-fill', 'black', '-draw', 'rectangle 1750,25 1855,100', target], check=True)
        result = subprocess.run(['magick', 'compare', '-metric', 'PAE', *masked, 'null:'],
                                capture_output=True, text=True)
        assert result.returncode in (0, 1), result.stderr
        match = re.search(r'\(([0-9.eE+-]+)\)', result.stderr)
        assert match, result.stderr
        maximum = float(match[1]) * 255
        print(f'{screen}: maximum channel difference={maximum:.2f}/255 (limit 2)')
        assert maximum <= 2, (screen, result.stderr)
