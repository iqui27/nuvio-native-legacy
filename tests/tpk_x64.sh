#!/bin/bash
# Verify the emulator artifact without installing it. Run tools/tpk.sh first.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import json
import struct
import xml.etree.ElementTree as ET
import zipfile

out = Path('build/tpk-x64')
version = json.loads(Path('deploy/app/appinfo.json').read_text())['version']
package = out / f'Nuvio-{version}-NuvioTpk-x64.tpk'
with zipfile.ZipFile(package) as z:
    elf = z.read('lib/libnuvio.so')
    assert elf[:6] == b'\x7fELF\x02\x01', 'expected little-endian ELF64'
    assert struct.unpack_from('<H', elf, 18)[0] == 62, 'expected EM_X86_64'
    assert elf == (out / 'libnuvio.so').read_bytes(), 'packaged library differs'
    assert 'lib/libnuvio_engine.so' not in z.namelist(), 'ARM P2P engine included'
    assert z.read('bin/NuvioTpk.dll'), 'missing .NET host'
    assert z.read('res/versao.txt').decode() == version
    manifest = ET.fromstring(z.read('tizen-manifest.xml'))
    assert manifest.attrib['version'] == version
    source = ET.parse('tizen-tpk/NuvioTpk/tizen-manifest.xml').getroot()
    source.set('version', version)
    assert ET.tostring(manifest) == ET.tostring(source), 'unexpected manifest change'
for source in Path('tizen-tpk').glob('*.cs'):
    assert source.read_bytes() == (out / 'host' / source.name).read_bytes(), f'stale host source: {source}'
print(f'ok: {package} contains the x86_64 library and current NuvioTpk host')
PY
