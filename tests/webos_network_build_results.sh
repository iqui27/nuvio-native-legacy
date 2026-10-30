#!/bin/bash
# Inspect build receipts without repeating full builds.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
root = Path('/Volumes/ExternalSSD/tmp/queda-webos-build')
for target in ('release', 'local', 'webos', 'android'):
    assert (root / f'{target}.rc').read_text().strip() == '0', target
    print(f'PASS: {target} build receipt rc=0')
for artifact in (root / 'nvbuild', Path('nuvio-proto.arm'),
                 Path('/Volumes/ExternalSSD/tmp/queda-webos-208/nuvio-proto.arm')):
    assert artifact.is_file() and artifact.stat().st_size > 0, artifact
apks = list(Path('build/android').glob('*-debug.apk'))
assert apks and all(p.stat().st_size > 0 for p in apks)
log = (root / 'android.log').read_text(errors='replace')
assert 'BUILD SUCCESSFUL' in log
assert 'UNALIGNED' not in log
assert 'android-apk-pages:' in log and '0 failed' in log
print('PASS: local/ARM/APK artifacts exist; Android 16 KB packaging check passed')
PY
