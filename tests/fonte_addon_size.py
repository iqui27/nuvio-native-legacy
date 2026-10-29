#!/usr/bin/env python3
"""Measure new packaged resources; platform binaries require their SDK builds."""
import gzip
import io
from pathlib import Path
import tarfile
import zipfile

root = Path(__file__).resolve().parents[1]
files = sorted((root / 'deploy/app/fonts').glob('addon-emoji*'))
assert len(files) == 2
raw = sum(f.stat().st_size for f in files)
for platform, prefix in [('Android APK', 'assets/fonts/'), ('Tizen WGT', 'app/fonts/'), ('Tizen TPK', 'res/fonts/')]:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as archive:
        for f in files:
            archive.writestr(prefix + f.name, f.read_bytes())
    print(f'{platform}: +{raw} unpacked resource bytes, +{len(buf.getvalue())} ZIP bytes')
buf = io.BytesIO()
with tarfile.open(fileobj=buf, mode='w') as archive:
    for f in files:
        entry = tarfile.TarInfo('fonts/' + f.name)
        entry.size = f.stat().st_size
        archive.addfile(entry, io.BytesIO(f.read_bytes()))
print(f'webOS IPK: +{raw} unpacked resource bytes, +{len(gzip.compress(buf.getvalue(), mtime=0))} gzip/tar bytes')
print('Resource contributions only; native/WASM code and full-package compression deltas not measured.')
