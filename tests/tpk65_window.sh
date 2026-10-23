#!/bin/bash
# Offline regression check for #612; no Tizen SDK or package restore required.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

source = Path('tizen-tpk/Program.cs').read_text()
baseline = 'abf3c0fa84411941b2735924094c37b2e9752b63'
original = subprocess.check_output(
    ['git', 'show', f'{baseline}:tizen-tpk/Program.cs'], text=True)
for project, symbol, transparent, opaque in [
    ('NuvioTpk60', 'NV_API8', 'false', 'true'),
    ('NuvioTpk65', 'NV_API9', 'true', 'false'),
    ('NuvioTpk', 'NV_API11', 'true', 'API11_GLVIEW'),
]:
    root = ET.parse(f'tizen-tpk/{project}/{project}.csproj')
    assert any(symbol in (node.text or '').split(';')
               for node in root.iter('DefineConstants')), project
    active = subprocess.run(
        ['clang', '-E', '-P', '-x', 'c', f'-D{symbol}', '-'],
        input=source, text=True, capture_output=True, check=True).stdout
    for flag, expected in [('TRANSPARENTE', transparent), ('OPACA', opaque)]:
        actual = re.search(r'const bool JANELA_PRINCIPAL_' + flag + r' = (\w+);', active)
        assert actual and actual[1] == expected, f'{project}: {flag} expected {expected}'
    print(f'PASS: {project}: transparent={transparent}, opaque={opaque}')
    if symbol != 'NV_API9':
        for extra in [[], ['-DNV_TEXTO']]:
            command = ['clang', '-E', '-P', '-x', 'c', f'-D{symbol}', *extra, '-']
            before, after = [subprocess.run(command, input=text, text=True,
                             capture_output=True, check=True).stdout
                             for text in [original, source]]
            assert before == after, f'{project}: unrelated preprocessed code changed'
program40 = 'tizen-tpk/NuvioTpk40/Program40.cs'
assert Path(program40).read_bytes() == subprocess.check_output(
    ['git', 'show', f'{baseline}:{program40}'])
print('PASS: other API source bytes unchanged (API8/API11 also with NV_TEXTO)')
assert 'JANELA_PRINCIPAL_TRANSPARENTE ? WindowMode.Transparent : WindowMode.Opaque' in source
assert re.search(r'if \(JANELA_PRINCIPAL_OPACA\)\s*\{\s*try \{ janela.SetOpaqueState\(true\);', source)
assert 'Janela("principal modo="' in source
assert 'Janela("principal SetOpaqueState: desligado")' in source
print('PASS: constructor, opaque guard and window diagnostics preserved')
PY
