#!/usr/bin/env python3
"""Prove the APK guard rejects omitted artwork and development-only paths."""
import pathlib
import subprocess
import tempfile
import zipfile
root = pathlib.Path.cwd()
with tempfile.TemporaryDirectory() as tmp:
    path = pathlib.Path(tmp)
    apk = path / 'fixture.apk'
    refs = []
    # Use the already staged, distributable Android artwork.
    stage = root / 'build/android/assets/art'
    assert stage.is_dir(), 'Run Android asset staging first'
    with zipfile.ZipFile(apk, 'w') as out:
        for p in stage.rglob('*'):
            if p.is_file() and p.name != 'plex.png':
                out.write(p, 'assets/art/' + str(p.relative_to(stage)))
    result = subprocess.run(['python3', 'tools/android-art-check.py', str(apk)], capture_output=True, text=True)
    assert result.returncode == 1 and 'marcas/plex.png' in result.stderr, result
    print('APK omitted Plex icon: FAIL as expected')
    with zipfile.ZipFile(apk, 'a') as out:
        out.write(stage / 'marcas/plex.png', 'assets/art/marcas/plex.png')
    subprocess.run(['python3', 'tools/android-art-check.py', str(apk)], check=True)
print('Android artwork omission FAIL -> PASS')
