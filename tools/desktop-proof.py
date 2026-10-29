#!/usr/bin/env python3
"""Verify existing desktop artifacts; never rebuild, publish, or install."""
import json
import os
from pathlib import Path
import plistlib
import re
import sys
import subprocess
import tempfile
import zipfile

out = Path(os.environ.get('NUVIO_DESKTOP_OUT', '/Volumes/ExternalSSD/tmp/rev/desktop'))
app = out / 'Nuvio.app'

def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT, timeout=60)

info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert info['CFBundleShortVersionString'] == '2.1'
assert info['SDL_FILESYSTEM_BASE_DIR_TYPE'] == 'resource'
assert set(run('lipo', '-archs', str(app / 'Contents/MacOS/Nuvio')).split()) == {'arm64', 'x86_64'}
run('codesign', '--verify', '--deep', '--strict', str(app))
for name in ['SDL2', 'SDL2_image', 'SDL2_ttf']:
    binary = app / f'Contents/Frameworks/{name}.framework/Versions/A/{name}'
    assert set(run('lipo', '-archs', str(binary)).split()) == {'arm64', 'x86_64'}
    deps = run('otool', '-L', str(binary))
    assert '/opt/homebrew/' not in deps and '/usr/local/' not in deps
assert '/opt/homebrew/' not in run('otool', '-L', str(app / 'Contents/MacOS/Nuvio'))

if '--mount-only' in sys.argv:
 with tempfile.TemporaryDirectory(prefix='nuvio-dmg-', dir=out) as mount:
    mounted = False
    try:
        result = run('hdiutil', 'attach', '-noverify', '-noautofsck', '-nobrowse', '-readonly',
                     str(out / 'Nuvio-mac-universal.dmg'))
        mounted = True
        mount = next(line.split('\t')[-1].strip() for line in result.splitlines() if '\t/Volumes/' in line)
        assert (Path(mount) / 'Nuvio.app/Contents/MacOS/Nuvio').is_file()
        assert (Path(mount) / 'Applications').is_symlink()
    finally:
        if mounted:
            run('hdiutil', 'detach', mount)
 print('desktop DMG: mounted application and Applications link OK')
 raise SystemExit(0)

run('7zz', 't', str(out / 'Nuvio-mac-universal.dmg'))
for p in [out / 'mac-login.png', out / 'windows-login.png']:
    assert p.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
mac_log = (out / 'mac-run-final.log').read_text()
win_log = (out / 'windows-run.log').read_text()
assert 'Library/Application Support/Nuvio/' in mac_log and 'tela=login' in mac_log
assert 'AppData/Roaming/Nuvio' in win_log.replace(chr(92), '/') and 'tela=login' in win_log
assert 'fontes: prontas' in win_log and 'http=200' in win_log
for root in [app / 'Contents/Resources/art', out / 'Nuvio-win64/art']:
    assert (root / 'marcas').is_dir()
    assert {p.name for p in root.glob('*.txt')} <= {'addons-recomendados.txt'}
    assert not (root / 'collections').exists()
    assert not (root / 'cache').exists()
    assert not (root / 'catalogo-rede.bin').exists()

with zipfile.ZipFile(out / 'Nuvio-win64.zip') as z:
    assert z.testzip() is None
    names = z.namelist()
    for name in ['Nuvio.exe', 'SDL2.dll', 'SDL2_image.dll', 'SDL2_ttf.dll', 'libcurl-x64.dll', 'libwinpthread-1.dll', 'README.md']:
        assert f'Nuvio-win64/{name}' in names
    assert not any(n.endswith('local.properties') or '/.env' in n or n.endswith('/env.h') for n in names)

# Audit all import tables, including transitive DLL dependencies. libcurl is
# loaded at runtime, so its import table must be checked explicitly too.
system = set('kernel32.dll user32.dll gdi32.dll ole32.dll oleaut32.dll shell32.dll '
             'advapi32.dll imm32.dll version.dll winmm.dll setupapi.dll msvcrt.dll '
             'ws2_32.dll bcrypt.dll crypt32.dll wldap32.dll normaliz.dll '
             'iphlpapi.dll opengl32.dll secur32.dll ucrtbase.dll cfgmgr32.dll '
             'comdlg32.dll winspool.drv shlwapi.dll ntdll.dll rpcrt4.dll usp10.dll'.split())
pkg = out / 'Nuvio-win64'
bundled = {p.name.lower() for p in pkg.glob('*.dll')}
imports = {}
for binary in [pkg / 'Nuvio.exe', *sorted(pkg.glob('*.dll'))]:
    table = run('x86_64-w64-mingw32-objdump', '-p', str(binary))
    assert 'pei-x86-64' in table
    deps = re.findall(r'DLL Name:\s*(\S+)', table)
    imports[binary.name] = deps
    missing = [d for d in deps if d.lower() not in system | bundled and not d.lower().startswith('api-ms-win-')]
    assert not missing, (binary.name, missing)
(out / 'dll-imports.json').write_text(json.dumps(imports, indent=2) + '\n')
print('desktop artifacts: universal architectures, signatures, DMG integrity, login PNGs, ZIP and all DLL imports OK')
