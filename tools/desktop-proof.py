#!/usr/bin/env python3
"""Audit existing desktop artifacts. Never rebuild, publish, or install."""
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import sys
import tempfile
import zipfile

out = Path(os.environ.get('NUVIO_DESKTOP_OUT', '/Volumes/ExternalSSD/tmp/rev/desktop'))
app = out / 'Nuvio.app'


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT, timeout=120)


def deps(binary):
    return [line.strip().split(' (compatibility')[0]
            for line in run('otool', '-L', str(binary)).splitlines()[1:]]


info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert info['CFBundleShortVersionString'] == '2.1'
assert info['SDL_FILESYSTEM_BASE_DIR_TYPE'] == 'resource'
metadata = json.loads((out / 'mac-mpv-deps.json').read_text())
architectures = set(metadata['architectures'])
assert set(run('lipo', '-archs', str(app / 'Contents/MacOS/Nuvio')).split()) == architectures
assert info['LSMinimumSystemVersion'] == metadata['minimum_macos']
run('codesign', '--verify', '--deep', '--strict', str(app))
frameworks = app / 'Contents/Frameworks'
assert (frameworks / 'libmpv.2.dylib').is_file()
for binary in [app / 'Contents/MacOS/Nuvio', *frameworks.glob('*.dylib'),
               *[frameworks / f'{n}.framework/Versions/A/{n}' for n in ['SDL2', 'SDL2_image', 'SDL2_ttf']]]:
    assert architectures <= set(run('lipo', '-archs', str(binary)).split()), binary
    for dep in deps(binary):
        assert not dep.startswith(('/opt/homebrew/', '/usr/local/')), (binary, dep)
        if dep.startswith('@loader_path/'):
            assert (binary.parent / dep.removeprefix('@loader_path/')).exists(), (binary, dep)
        elif dep.startswith('@rpath/'):
            assert (frameworks / dep.removeprefix('@rpath/')).exists(), (binary, dep)
        else:
            assert dep.startswith(('/System/Library/', '/usr/lib/')), (binary, dep)

label = 'universal' if len(architectures) == 2 else next(iter(architectures))
dmg = out / f'Nuvio-mac-{label}.dmg'
if '--mount-only' in sys.argv:
    mounted = None
    try:
        result = run('hdiutil', 'attach', '-noverify', '-noautofsck', '-nobrowse', '-readonly', str(dmg))
        mounted = next(line.split('\t')[-1].strip() for line in result.splitlines() if '\t/Volumes/' in line)
        assert (Path(mounted) / 'Nuvio.app/Contents/MacOS/Nuvio').is_file()
        assert (Path(mounted) / 'Applications').is_symlink()
    finally:
        if mounted:
            run('hdiutil', 'detach', mounted)
    print('desktop DMG: mounted application and Applications link OK')
    raise SystemExit(0)
run('7zz', 't', str(dmg))
for name in ['mac-home-1280.png', 'mac-home-narrow.png', 'mock-vs-app.png', 'mac-video.png']:
    assert (out / name).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), name
assert '1280' in run('sips', '-g', 'pixelWidth', str(out / 'mac-home-1280.png'))
assert '800' in run('sips', '-g', 'pixelHeight', str(out / 'mac-home-1280.png'))
for root in [app / 'Contents/Resources/art', out / 'Nuvio-win64/art']:
    assert (root / 'marcas').is_dir()
    assert not (root / 'catalogo-rede.bin').exists()
    assert not (root / 'catalogo.txt').exists()
    assert not (root / 'cache').exists()

with zipfile.ZipFile(out / 'Nuvio-win64.zip') as z:
    assert z.testzip() is None
    names = z.namelist()
    for name in ['Nuvio.exe', 'SDL2.dll', 'SDL2_image.dll', 'SDL2_ttf.dll',
                 'libcurl-x64.dll', 'libwinpthread-1.dll', 'mpv-2.dll', 'README.md']:
        assert f'Nuvio-win64/{name}' in names
    assert not any(n.endswith('local.properties') or '/.env' in n or n.endswith('/env.h') for n in names)

system = set('kernel32.dll user32.dll gdi32.dll ole32.dll oleaut32.dll shell32.dll '
             'advapi32.dll imm32.dll version.dll winmm.dll setupapi.dll msvcrt.dll '
             'ws2_32.dll bcrypt.dll crypt32.dll wldap32.dll normaliz.dll '
             'iphlpapi.dll opengl32.dll secur32.dll ucrtbase.dll cfgmgr32.dll '
             'comdlg32.dll winspool.drv shlwapi.dll ntdll.dll rpcrt4.dll usp10.dll '
             'avrt.dll dwmapi.dll dxgi.dll d3d11.dll d3d9.dll d3d12.dll dxva2.dll '
             'powrprof.dll hid.dll winhttp.dll mfplat.dll mf.dll mfuuid.dll '
             'propsys.dll ksuser.dll oleacc.dll shcore.dll userenv.dll ncrypt.dll '
             'wintrust.dll imagehlp.dll msimg32.dll dnsapi.dll dsound.dll '.split())
pkg = out / 'Nuvio-win64'
bundled = {p.name.lower() for p in pkg.glob('*.dll')}
imports = {}
for binary in [pkg / 'Nuvio.exe', *sorted(pkg.glob('*.dll'))]:
    table = run('x86_64-w64-mingw32-objdump', '-p', str(binary))
    assert 'pei-x86-64' in table
    dependencies = re.findall(r'DLL Name:\s*(\S+)', table)
    imports[binary.name] = dependencies
    missing = [d for d in dependencies if d.lower() not in system | bundled and not d.lower().startswith(('api-ms-win-', 'ext-ms-win-'))]
    assert not missing, (binary.name, missing)
assert any('mpv' in d.lower() for d in imports['Nuvio.exe'])
(out / 'dll-imports.json').write_text(json.dumps(imports, indent=2) + '\n')
print('desktop artifacts: mpv closure, architectures, signatures, DMG, PNGs, ZIP and Windows imports OK')
