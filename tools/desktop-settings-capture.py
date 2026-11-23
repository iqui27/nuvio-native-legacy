#!/usr/bin/env python3
"""Approved Mac Settings proof, isolated data, background SDL input and GL readback."""
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'codigo'
TMP = Path('/Volumes/ExternalSSD/tmp/jobtmp')
BOARD = BASE / 'boards/aprovadas/Mac · ajustes.png'
BINARY = Path(os.environ.get('NV_DESKTOP_SETTINGS_BIN', TMP / 'Nuvio-ajustes'))
OUT.mkdir(parents=True, exist_ok=True)
RUNTIME = Path(tempfile.mkdtemp(prefix='nuvio-mac-ajustes-', dir='/private/tmp'))
APP = RUNTIME / 'Nuvio.app'
MACOS = APP / 'Contents/MacOS'
MACOS.mkdir(parents=True)
shutil.copy2(BINARY, MACOS / 'Nuvio')
LOG = RUNTIME / 'runtime.log'
DATA = RUNTIME / 'data'
IPC = RUNTIME / 'ipc'
environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
                   NV_CAPTURA_GL='1', NV_CAPTURA_DIR=str(IPC), SDL_MAC_BACKGROUND_APP='1',
                   SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
(APP / 'Contents/Info.plist').write_bytes(plistlib.dumps(dict(
    CFBundleExecutable='Nuvio', CFBundleIdentifier='app.nuvio.ajustes-proof',
    CFBundleName='Nuvio', CFBundlePackageType='APPL', LSUIElement=True,
    NSHighResolutionCapable=True)))
subprocess.run(['codesign', '--force', '--sign', '-', str(APP)], check=True, capture_output=True)
ART = RUNTIME / 'resources/art'
# Start empty. Only deterministic, account-free proof preferences follow.
DATA.mkdir(mode=0o700)
assert DATA.is_dir() and DATA.resolve().parent == RUNTIME.resolve()
probe = DATA / '.isolation-probe'
probe.write_text('isolated\n')
probe.unlink()
shutil.copytree(BASE / 'scene-resources', ART.parent)
(ART / 'desktop').mkdir(exist_ok=True)
shutil.copy2(ROOT / 'design/desktop/ajustes-background.jpg', ART / 'desktop/ajustes-background.jpg')
shutil.copytree(ROOT / 'deploy/app/fonts', ART.parent / 'fonts', dirs_exist_ok=True)
for source in [*ROOT.glob('src/novidades*.*'), *ROOT.glob('src/novidades/*.inc')]:
    for name in re.findall(r'"([^"/]*\.txt)"', source.read_text()):
        (DATA / name).write_text('1\n')
for name in ['salvos-intro.txt', 'aviso-log.txt', 'recintro-social.txt']:
    (DATA / name).write_text('1\n')
(DATA / 'dicas-controle-205.txt').write_text('2.1\n')
(DATA / 'diagnostico-otimizacao-intro.cfg').write_text('versao=1\n')
hero = RUNTIME / 'design/glass-ilha/arte.jpg'
hero.parent.mkdir(parents=True)
subprocess.run(['magick', str(BASE / 'boards/ajustes-fallout-art.png'), str(hero)], check=True)
fixture = RUNTIME / 'scene'
subprocess.run(['cc', '-Isrc', 'tools/desktop-scene.c', '-o', str(fixture)], cwd=ROOT, check=True)
subprocess.run([str(fixture), str(DATA / 'catalogo-rede.bin'), str(ART), str(RUNTIME)], check=True)
# Match the catalog cache's locale to the Portuguese board fixture.
catalog = bytearray((DATA / 'catalogo-rede.bin').read_bytes())
struct.pack_into('<i', catalog, 92, 0)
catalog[96:112] = b'pt-BR' + b'\0' * 11
(DATA / 'catalogo-rede.bin').write_bytes(catalog)
# Proof preferences only. Never touch the owner's profile or a connected account.
for path in (DATA / 'ajustes.txt', DATA / 'ajustes-p1.txt'):
    path.write_text('idioma 0\nidiomaAutoLocal 0\nselected_theme 2\ntamanhoUiLocal 8\n'
                    'tamanhoAjustesLocal 2\nescalaTextoLocal 0\nvidroLocal 2\nvidroOpacLocal 0\n')
(DATA / 'perfis.txt').write_text('1\t0\t1\t0\t#777B83\tPerfil local\t\t\t0\n')
(DATA / 'aparencia-20.txt').write_text('1\n')
(DATA / 'seguro.txt').unlink(missing_ok=True)
# The remote 2.0.8 release notice has already been seen in this offline fixture.
(DATA / 'atualizacao-vista.txt').write_text('2.0.8\n')
(DATA / 'desktop-sidebar.txt').write_text('0\n')
helper_source = RUNTIME / 'front.m'
helper_source.write_text('#import <Cocoa/Cocoa.h>\nint main(void) { @autoreleasepool { printf("%d\\n", NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier); } }\n')
helper = RUNTIME / 'front'
subprocess.run(['cc', str(helper_source), '-framework', 'Cocoa', '-o', str(helper)], check=True)
def front():
    return int(subprocess.check_output([str(helper)]))
IPC.mkdir()
def command(value):
    pending = IPC / 'pending'
    pending.write_text(value + '\n')
    pending.replace(IPC / 'nuvio-key')
    deadline = time.monotonic() + 60
    while (IPC / 'nuvio-key').stat().st_size:
        assert time.monotonic() < deadline, value
        time.sleep(.05)
    time.sleep(.6)
    assert front() != pid, 'capture activated during ' + value
def photo(name):
    source = IPC / 'nuvio-shot.bmp'
    source.unlink(missing_ok=True)
    (IPC / 'nuvio-shot-req').write_text('1\n')
    deadline = time.monotonic() + 60
    while True:
        bitmap = source.read_bytes() if source.exists() else b''
        if len(bitmap) >= 54 and len(bitmap) == struct.unpack_from('<I', bitmap, 2)[0]:
            break
        assert time.monotonic() < deadline, 'GL readback timeout'
        time.sleep(.1)
    w, h = struct.unpack_from('<ii', bitmap, 18)
    assert abs(w / h - 1440 / 900) < .001
    dest = OUT / (name + '.png')
    subprocess.run(['magick', '-size', f'{w}x{h}', '-depth', '8', 'bgra:-', '-flip',
                    '-resize', '1440x900', str(dest)], input=bitmap[54:], check=True)
    return dest
foreground = front()
pid = None
try:
    launch = ['open', '-n', '-g', '-j', '--stdout', str(LOG), '--stderr', str(RUNTIME / 'stderr.log')]
    for key, value in environment.items():
        launch += ['--env', key + '=' + value]
    subprocess.run(launch + [str(APP), '--args', str(ART)], check=True)
    deadline = time.monotonic() + 60
    while pid is None:
        matches = subprocess.run(['pgrep', '-f', '^' + re.escape(str(MACOS / 'Nuvio'))], capture_output=True, text=True)
        if matches.returncode == 0:
            pid = int(matches.stdout.splitlines()[-1])
        assert time.monotonic() < deadline, 'background launch timeout'
        time.sleep(.1)
    while not LOG.exists() or 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
        assert time.monotonic() < deadline, 'first frame timeout'
        time.sleep(.2)
    assert front() != pid
    assert '[dados] gravando em ' + str(DATA) + '\n' in LOG.read_text(errors='replace')
    command('appearance:DarkAqua')
    command('janela:1440,900')
    command('preview:settings:0')
    command('mover:1380,880')
    time.sleep(3)
    png = photo('ajustes-1440x900')
    assert front() != pid
    subprocess.run(['magick', str(BOARD), str(png), '+append', '-quality', '85', str(OUT / 'ajustes-lado.jpg')], check=True)
    subprocess.run(['magick', str(OUT / 'ajustes-lado.jpg'), '-resize', '1000x1000', '-quality', '80', str(TMP / 'ajustes-lado-small.jpg')], check=True)
finally:
    if pid is None:
        matches = subprocess.run(['pgrep', '-f', '^' + re.escape(str(MACOS / 'Nuvio'))], capture_output=True, text=True)
        if matches.returncode == 0:
            pid = int(matches.stdout.splitlines()[-1])
    if pid:
        try:
            os.kill(pid, signal.SIGTERM)
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                try:
                    os.kill(pid, 0)
                except ProcessLookupError:
                    break
                time.sleep(.05)
            else:
                os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    (OUT / 'ajustes-focus.json').write_text(json.dumps(dict(before=foreground, after=front(), proof_pid=pid, runtime=str(RUNTIME)), indent=2))
assert front() != pid
assert subprocess.run(['pgrep', '-f', '^' + re.escape(str(MACOS / 'Nuvio'))], capture_output=True).returncode == 1
print('PASS: 1440x900 GL readback, SDL input, background-only launch, proof process stopped')
print(OUT / 'ajustes-lado.jpg')
