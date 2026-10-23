#!/bin/bash
# Exercise release verification without Android SDKs or release credentials.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import tempfile
from unittest.mock import patch
import zipfile

from tools import release

version = '2.0.7'
artifacts = {
    f'Nuvio-{version}-android.apk': ('arm64-v8a', 'armeabi-v7a'),
    f'Nuvio-{version}-android-lite-armv7.apk': ('armeabi-v7a',),
    f'Nuvio-{version}-android-lite-arm64.apk': ('arm64-v8a',),
}
values = {'NV_TEST': 'release-config-sentinel'}
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp)
    for name, abis in artifacts.items():
        with zipfile.ZipFile(out / name, 'w') as apk:
            for abi in abis:
                apk.writestr(f'lib/{abi}/libmain.so', values['NV_TEST'])
    with patch.object(release, 'expected_files', return_value=list(artifacts)):
        report = release.verify_packages(out, version, values)
    assert set(report) == set(artifacts)
    for name, abis in artifacts.items():
        assert set(report[name]['binaries']) == {f'lib/{abi}/libmain.so' for abi in abis}
        assert all(binary['config'] == '1/1' for binary in report[name]['binaries'].values())
        assert report[name]['sha256'] == release.digest(out / name)
        print(f'PASS: {name}: expected ABI libraries verified')
PY
