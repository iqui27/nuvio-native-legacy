#!/usr/bin/env python3
"""Round 13 proof: desktop Settings before/after at 1440x900 and 900x700.
SDL input to the app's own window and GL readback only; never activates the app."""
import json
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'r13'
import sys
SOURCE_APP = Path(sys.argv[1]) if len(sys.argv) > 1 else OUT / 'Nuvio.app'
TAG = sys.argv[2] if len(sys.argv) > 2 else 'after'
RUNTIME = Path(tempfile.mkdtemp(prefix='nuvio-desktop-r13-', dir='/private/tmp'))
OUT.mkdir(parents=True, exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
subprocess.run(['ditto', str(SOURCE_APP), str(APP)], check=True)
DATA = RUNTIME / 'data'
ART = RUNTIME / 'resources/art'
subprocess.run(['ditto', str(BASE / 'scene-data'), str(DATA)], check=True)
subprocess.run(['ditto', str(BASE / 'scene-resources'), str(ART.parent)], check=True)
hero = RUNTIME / 'design/glass-ilha/arte.jpg'
hero.parent.mkdir(parents=True, exist_ok=True)
hero.write_bytes((ROOT / 'design/glass-ilha/arte.jpg').read_bytes())
fixture = RUNTIME / 'scene'
subprocess.run(['cc', '-Isrc', 'tools/desktop-scene.c', '-o', str(fixture)], cwd=ROOT, check=True)
subprocess.run([str(fixture), str(DATA / 'catalogo-rede.bin'), str(ART), str(RUNTIME)], check=True)
(DATA / 'seguro.txt').unlink(missing_ok=True)
(DATA / 'atualizacao-vista.txt').write_text('2.0.8\n')
(DATA / 'desktop-sidebar.txt').write_text('0\n')
LOG = OUT / ('runtime-' + TAG + '.log')
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
HOVER = ('760,330',)
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
    for size in ((1440,900),(900,700)):
        w,h=size
        command(f'janela:{w},{h}',2)
        for section,name in ((0,'playback'),(5,'home'),(6,'appearance'),(10,'device')):
            command('preview:settings',2)
            for _ in range(14): command('up',.15)
            for _ in range(section): command('down',.3)
            time.sleep(1.5)
            shots.append(photo(f'settings-{name}-{w}x{h}-{TAG}',w,h))
            assert front() != pid
    if TAG == 'after':
        command('janela:1440,900',2)
        command('preview:settings',2)
        for _ in range(14): command('up',.15)
        command('right',.6)
        for spot in HOVER: command('mover:'+spot,.4)
        time.sleep(.8)
        shots.append(photo('settings-hover-1440x900',1440,900))
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
    (OUT/('focus-'+TAG+'.json')).write_text(json.dumps(dict(before=foreground,after=front(),proof_pid=pid)))
    (OUT/('captures-'+TAG+'.json')).write_text(json.dumps(dict(shots=shots,
),indent=2))
assert front() != pid
assert subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True).returncode==1
print('PASS: r13 captures', TAG, '; proof app never foreground; proof process stopped')
