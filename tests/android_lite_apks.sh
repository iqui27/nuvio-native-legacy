#!/bin/bash
# Verify the signed per-ABI Lite artifacts produced by tools/android.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
BT="$SDK/build-tools/35.0.0"
python3 - "$BT" <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

version = json.loads(Path('deploy/app/appinfo.json').read_text())['version']
for arch, abi in [('armv7', 'armeabi-v7a'), ('arm64', 'arm64-v8a')]:
    apk = Path(f'build/android/Nuvio-{version}-android-lite-{arch}.apk')
    certs = subprocess.check_output([f'{sys.argv[1]}/apksigner', 'verify', '--print-certs', str(apk)], text=True)
    assert 'Signer #1 certificate SHA-256 digest: c3c967d4fac138de15126da01ea3ae43b52e6aa23106e336c2acc607ae0f2add' in certs
    with zipfile.ZipFile(apk) as z:
        assert {n.split('/')[1] for n in z.namelist() if n.startswith('lib/')} == {abi}
        main = z.read(f'lib/{abi}/libmain.so')
        assert f'-android-lite-{arch}.apk'.encode() in main
        assert b'Nuvio Engine/' not in main
    size = apk.stat().st_size
    print(f'{apk}: {size} bytes ({size / 1_000_000:.2f} MB); signature, ABI, updater identity OK')
    print(f'{hashlib.sha256(apk.read_bytes()).hexdigest()}  {apk.name}')
PY
