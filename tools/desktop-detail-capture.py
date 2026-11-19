#!/usr/bin/env python3
"""Detail 1440x900 capture vs the approved Mac board. No activation; SDL input + GL readback.
NUVIO_DESKTOP_APP overrides the packaged app (default: shared desktop out)."""
import atexit
import json
import os
from pathlib import Path
import re
import signal
import shutil
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = Path(os.environ.get('NUVIO_DETAIL_OUT', BASE / 'codigo'))
BOARD = BASE / 'boards/aprovadas/Mac · detalhe.png'
RUNTIME = Path(tempfile.mkdtemp(prefix='nuvio-desktop-detail-', dir='/private/tmp'))
atexit.register(shutil.rmtree, RUNTIME, ignore_errors=True)
OUT.mkdir(parents=True, exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
subprocess.run(['ditto', os.environ.get('NUVIO_DESKTOP_APP', str(BASE / 'Nuvio.app')), str(APP)], check=True)
DATA = RUNTIME / 'data'
ART = RUNTIME / 'resources/art'
subprocess.run(['ditto', str(BASE / 'scene-data'), str(DATA)], check=True)
subprocess.run(['ditto', str(BASE / 'scene-resources'), str(ART.parent)], check=True)
hero = RUNTIME / 'design/glass-ilha/arte.jpg'
hero.parent.mkdir(parents=True, exist_ok=True)
hero.write_bytes((ART / '00.jpg').read_bytes())
# Match the approved board's language in this disposable proof profile only.
for name in ('ajustes.txt', 'ajustes-p1.txt'):
    path=DATA / name
    text=path.read_text() if path.exists() else ''
    text=re.sub(r'^idioma\s+.*$', 'idioma 0', text, flags=re.M)
    text+='\nidioma 0\nidiomaAutoLocal 0\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\nfundoLocal 0\ndetalheVeu 100\n'
    path.write_text(text)
# Pin the approved artwork through the app's existing per-profile art choice.
for profile in (0,1):
    (DATA / f'arte-escolhida-p{profile}.txt').write_text(f'tt12637874\t{ART / "00.jpg"}\t{ART / "logo/00.png"}\n')
fixture = RUNTIME / 'scene'
scene=RUNTIME / 'scene.c'
scene.write_text((ROOT / 'tools/desktop-scene.c').read_text().replace('"", 1, 1, "en-US"','"", 1, 0, "pt-BR"'))
subprocess.run(['cc', '-Isrc', str(scene), '-o', str(fixture)], cwd=ROOT, check=True)
subprocess.run([str(fixture), str(DATA / 'catalogo-rede.bin'), str(ART), str(RUNTIME)], check=True)
(DATA / 'seguro.txt').unlink(missing_ok=True)
(DATA / 'atualizacao-vista.txt').write_text('2.0.8\n')
(DATA / 'desktop-sidebar.txt').write_text('0\n')
LOG = OUT / 'runtime.log'
LOG.write_text('')
executable = APP / 'Contents/MacOS/Nuvio'
binary = executable.with_name('Nuvio-proof-bin')
executable.replace(binary)
executable.write_text('#!/bin/bash\nexec "' + str(binary) + '" "$@" > "' + str(LOG) + '" 2>&1\n')
executable.chmod(0o755)
subprocess.run(['codesign', '--force', '--sign', '-', str(APP)], check=True, capture_output=True)
source = RUNTIME / 'front.m'
source.write_text('''#import <Cocoa/Cocoa.h>
int main(void) { @autoreleasepool { printf("%d\\n",NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier); } }
''')
helper = RUNTIME / 'front'
subprocess.run(['cc', str(source), '-framework', 'Cocoa', '-o', str(helper)], check=True)
def front():
    return int(subprocess.check_output([str(helper)]))
foreground = front()
ipc = RUNTIME / 'ipc'
ipc.mkdir()
def command(value, delay=.5):
    staging = ipc / 'pending'
    staging.write_text(value + '\n')
    staging.replace(ipc / 'nuvio-key')
    deadline = time.monotonic() + 45
    while (ipc / 'nuvio-key').stat().st_size:
        if time.monotonic() > deadline:
            raise RuntimeError('Input timeout: ' + value)
        time.sleep(.02)
    time.sleep(delay)
def photo(name, width, height):
    source = ipc / 'nuvio-shot.bmp'
    source.unlink(missing_ok=True)
    (ipc / 'nuvio-shot-req').write_text('1\n')
    deadline = time.monotonic() + 45
    while True:
        if source.exists():
            bitmap = source.read_bytes()
            if len(bitmap) >= 54 and len(bitmap) == struct.unpack_from('<I', bitmap, 2)[0]:
                break
        if time.monotonic() > deadline:
            raise RuntimeError('Capture timeout')
        time.sleep(.1)
    w, h = struct.unpack_from('<ii', bitmap, 18)
    assert abs(w/h - width/height) < .001, (w,h,width,height)
    dest = OUT / (name + '.png')
    subprocess.run(['magick', '-size', f'{w}x{h}', '-depth', '8', 'bgra:-', '-flip',
                    '-resize', f'{width}x{height}', str(dest)], input=bitmap[54:], check=True)

    assert struct.unpack('>II', dest.read_bytes()[16:24]) == (width,height)
    print(dest, flush=True)
    return dict(file=str(dest), drawable=[w,h], output=[width,height])
environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
                   NV_CAPTURA_GL='1', NV_CAPTURA_DIR=str(ipc), SDL_MAC_BACKGROUND_APP='1',
                   SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
launch = ['open', '-n', '-g', '-j']
for key, value in environment.items():
    launch += ['--env', key + '=' + value]
launch += ['-a', str(APP), '--args', str(ART)]
pid = None
shots = []
try:
    subprocess.run(launch, check=True)
    for attempt in range(100):
        matches=subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True,text=True)
        if matches.returncode==0:
            pid=int(matches.stdout.splitlines()[-1]);break
        time.sleep(.05)
    deadline = time.monotonic() + 60
    while 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
        if time.monotonic() > deadline:
            raise RuntimeError('Background launch timeout')
        time.sleep(.2)
    pid = int(subprocess.check_output(['pgrep','-f','^'+re.escape(str(binary))],text=True).splitlines()[-1])
    assert front() != pid
    command('appearance:DarkAqua')
    command('back')
    command('janela:1440,900',2)
    command('abrir:tt12637874',4)
    assert 'detalhe: foco/abertura' in LOG.read_text(errors='replace'), 'Detail route did not open'
    shots.append(photo('detalhe-app',1440,900))
    for i,y in enumerate((-600,-1400)):
        command(f'wheel:900,600,0,{y},0',2)
        shots.append(photo(f'detalhe-app-rolado{i+1}',1440,900))
    assert front() != pid
finally:
    if pid:
        try:
            os.kill(pid,signal.SIGTERM)
            deadline=time.monotonic()+4
            while time.monotonic()<deadline:
                try: os.kill(pid,0)
                except ProcessLookupError: break
                time.sleep(.05)
            else: os.kill(pid,signal.SIGKILL)
        except ProcessLookupError: pass
    (OUT/'detalhe-focus.json').write_text(json.dumps(dict(before=foreground,after=front(),proof_pid=pid)))
    (OUT/'detalhe-captures.json').write_text(json.dumps(dict(shots=shots),indent=2))
assert front() != pid
assert subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True).returncode==1
subprocess.run(['magick', str(BOARD), '-resize', '1440x900!', str(OUT / 'detalhe-app.png'), '+append',
                '-quality', '82', str(OUT / 'detalhe-lado.jpg')], check=True)
print('PASS: detail captured; app never foreground; process stopped;', OUT / 'detalhe-lado.jpg')
