#!/usr/bin/env python3
"""Focused, nonactivating desktop proof; uses only the app's SDL input port."""
import argparse
import json
import struct
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'r6'
parser = argparse.ArgumentParser()
parser.add_argument('stage', choices=('before', 'after'))
args = parser.parse_args()
OUT.mkdir(parents=True, exist_ok=True)
# LaunchServices children lack removable-volume access on this host.
# Keep runtime state in a private temporary folder; export only proof images.
RUNTIME = Path('/private/tmp/nuvio-desktop-r6')
RUNTIME.mkdir(exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
if not APP.exists():
    subprocess.run(['ditto', str(BASE / 'Nuvio.app'), str(APP)], check=True)
DATA = RUNTIME / 'data'
if not DATA.exists():
    subprocess.run(['ditto', str(BASE / 'scene-data'), str(DATA)], check=True)
ART = RUNTIME / 'resources/art'
if not ART.exists():
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
LOG = RUNTIME / f'{args.stage}-runtime.log'
LOG.write_text('')
# LaunchServices on this host rejects --stdout/--stderr with -10810.
# Redirect only inside the isolated proof bundle.
executable = APP / 'Contents/MacOS/Nuvio'
binary = executable.with_name('Nuvio-proof-bin')
if not binary.exists():
    executable.rename(binary)
executable.write_text('#!/bin/bash\nexec "' + str(binary) + '" "$@" > "' + str(LOG) + '" 2>&1\n')
executable.chmod(0o755)
subprocess.run(['codesign', '--force', '--sign', '-', str(APP)], check=True, capture_output=True)

with tempfile.TemporaryDirectory(prefix='desktop-r6-', dir=RUNTIME) as temporary:
    ipc = Path(temporary)
    def command(value, delay=1):
        pending = ipc / 'nuvio-key'
        staging = ipc / 'key-pending'
        staging.write_text(value + '\n')
        staging.replace(pending)
        deadline = time.monotonic() + 45
        while pending.exists() and pending.stat().st_size:
            if time.monotonic() > deadline:
                raise RuntimeError('Input timed out: ' + value)
            time.sleep(.05)
        time.sleep(delay)

    def photo(name):
        source = ipc / 'nuvio-shot.bmp'
        source.unlink(missing_ok=True)
        (ipc / 'nuvio-shot-req').write_text('1\n')
        deadline = time.monotonic() + 45
        while not source.exists():
            if time.monotonic() > deadline:
                raise RuntimeError('GL readback timed out')
            time.sleep(.1)
        target = OUT / f'{args.stage}-{name}-1920x1080.png'
        bitmap = source.read_bytes()
        width, height = struct.unpack_from('<ii', bitmap, 18)
        pixels = bitmap[54:]
        # Preserve GL alpha: BMP readers otherwise discard the fourth byte.
        subprocess.run(['magick', '-size', f'{width}x{height}', '-depth', '8',
                        'bgra:-', '-flip', '-resize', '1920x1080!', str(target)],
                       input=pixels, check=True)
        sidebar_alpha = pixels[((height // 2) * width + int(width * .05)) * 4 + 3]
        content_alpha = pixels[((height // 2) * width + int(width * .70)) * 4 + 3]
        target.with_suffix('.json').write_text(json.dumps(dict(
            drawable=[width, height], sidebar_alpha=sidebar_alpha,
            content_alpha=content_alpha)))
        print(target, flush=True)

    environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1',
                       NV_CAPTURA='1', NV_CAPTURA_DIR=str(ipc),
                       SDL_MAC_BACKGROUND_APP='1', SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
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
        processes = subprocess.check_output(['pgrep', '-f', '^' + re.escape(str(binary))], text=True)
        pid = int(processes.splitlines()[-1])
        command('janela:1920,1080')
        command('appearance:DarkAqua')
        command('back')
        command('preview:home', 3)
        command('preview:expand', 2)
        command('mover:1800,100')
        photo('home')
        window = re.findall(r'\[desktop-native\] window=(\d+)', LOG.read_text(errors='replace'))[-1]
        native = OUT / f'{args.stage}-native-glass.png'
        subprocess.run(['screencapture', '-l', window, '-o', '-x', str(native)], check=True)
        subprocess.run(['magick', str(native), '-resize', '1920x1080!', str(native)], check=True)
        command('preview:library')
        command('abrir:tt12637874', 4)
        photo('detail-top')
        command('wheel:1100,800,0,-1500', 3)
        photo('detail-scrolled')
        command('wheel:1100,800,0,-650', 3)
        photo('detail-lower')
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
        if LOG.exists():
            (OUT / LOG.name).write_bytes(LOG.read_bytes())
