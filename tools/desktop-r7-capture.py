#!/usr/bin/env python3
"""Background gesture proof: app-local SDL input and GL readback only."""
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
OUT = BASE / 'r7'
RUNTIME = Path('/private/tmp/nuvio-desktop-r7')
OUT.mkdir(parents=True, exist_ok=True)
RUNTIME.mkdir(exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
subprocess.run(['ditto', str(BASE / 'Nuvio.app'), str(APP)], check=True)
DATA = RUNTIME / 'data'
ART = RUNTIME / 'resources/art'
subprocess.run(['ditto', str(BASE / 'scene-data'), str(DATA)], check=True)
if not ART.parent.exists():
    # Clone existing local artwork when available; otherwise stage the fixture.
    previous = Path('/private/tmp/nuvio-desktop-r6/resources')
    if previous.exists():
        subprocess.run(['cp', '-cR', str(previous), str(ART.parent)], check=True)
    else:
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
LOG = RUNTIME / 'runtime.log'
LOG.write_text('')
executable = APP / 'Contents/MacOS/Nuvio'
binary = executable.with_name('Nuvio-proof-bin')
executable.replace(binary)
executable.write_text('#!/bin/bash\nexec "' + str(binary) + '" "$@" > "' + str(LOG) + '" 2>&1\n')
executable.chmod(0o755)
subprocess.run(['codesign', '--force', '--sign', '-', str(APP)], check=True, capture_output=True)

with tempfile.TemporaryDirectory(prefix='desktop-r7-', dir=RUNTIME) as temporary:
    ipc = Path(temporary)

    def command(value):
        pending = ipc / 'nuvio-key'
        staging = ipc / 'pending'
        staging.write_text(value + '\n')
        staging.replace(pending)
        deadline = time.monotonic() + 45
        while pending.exists() and pending.stat().st_size:
            if time.monotonic() > deadline:
                raise RuntimeError('Input timed out: ' + value)
            time.sleep(.05)
        time.sleep(1)

    def photo(name):
        source = ipc / 'nuvio-shot.bmp'
        source.unlink(missing_ok=True)
        (ipc / 'nuvio-shot-req').write_text('1\n')
        deadline = time.monotonic() + 45
        while not source.exists():
            if time.monotonic() > deadline:
                raise RuntimeError('GL readback timed out')
            time.sleep(.1)
        bitmap = source.read_bytes()
        width, height = struct.unpack_from('<ii', bitmap, 18)
        target = OUT / (name + '.png')
        subprocess.run(['magick', '-size', f'{width}x{height}', '-depth', '8',
                        'bgra:-', '-flip', '-resize', '1920x1080!', str(target)],
                       input=bitmap[54:], check=True)
        png = target.read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n'
        assert struct.unpack('>II', png[16:24]) == (1920, 1080)
        print(target, flush=True)

    environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
                       NV_CAPTURA_DIR=str(ipc), SDL_MAC_BACKGROUND_APP='1',
                       SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
    launch = ['open', '-n', '-g', '-j']
    for key, value in environment.items():
        launch += ['--env', key + '=' + value]
    launch += ['-a', str(APP), '--args', str(ART)]
    pid = None
    try:
        subprocess.run(launch, check=True)
        deadline = time.monotonic() + 45
        while 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
            if time.monotonic() > deadline:
                raise RuntimeError('Background launch timed out')
            time.sleep(.2)
        pid = int(subprocess.check_output(['pgrep', '-f', '^' + re.escape(str(binary))], text=True).splitlines()[-1])
        command('janela:1920,1080')
        command('appearance:DarkAqua')
        command('back')
        command('preview:home')
        command('wheel:1915,400,0,0,33')
        command('wheel:1750,400,-60,0,32')
        command('wheel:1750,400,0,0,34')
        photo('gesture-right-panel')
        command('wheel:1700,400,60,0,33')
        command('wheel:1700,400,0,0,34')
        photo('gesture-panel-closed')
        command('wheel:1915,400,-60,0,33')
        command('wheel:1750,400,0,0,34')
        command('back')
        photo('escape-panel-closed')
        opened = OUT / 'gesture-right-panel.png'
        for name in ('gesture-panel-closed', 'escape-panel-closed'):
            result = subprocess.run(['magick', 'compare', '-metric', 'RMSE',
                                     str(opened), str(OUT / (name + '.png')), 'null:'],
                                    capture_output=True, text=True)
            assert result.returncode == 1, result.stderr
            assert float(result.stderr.split('(')[1].split(')')[0]) > .03, result.stderr
    finally:
        if pid is None:
            result = subprocess.run(['pgrep', '-f', '^' + re.escape(str(binary))], capture_output=True, text=True)
            if result.returncode == 0:
                pid = int(result.stdout.splitlines()[-1])
        if pid:
            os.kill(pid, signal.SIGTERM)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    os.kill(pid, 0)
                except ProcessLookupError:
                    break
                time.sleep(.1)
            else:
                os.kill(pid, signal.SIGKILL)
        (OUT / 'runtime.log').write_bytes(LOG.read_bytes())
assert subprocess.run(['pgrep', '-f', '^' + re.escape(str(binary))],
                      capture_output=True).returncode == 1
print('PASS: gesture opened existing panel; reverse and Esc closed it; GL PNGs; proof instance stopped')
