#!/usr/bin/env python3
"""Capture the packaged Mac app with an isolated, offline catalog fixture."""
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('NUVIO_DESKTOP_OUT', '/Volumes/ExternalSSD/tmp/rev/desktop'))
MOCK = Path('/Volumes/ExternalSSD/nv-21-21-iosmock')
DATA = OUT / 'scene-data'
ART = OUT / 'scene-resources/art'
APP = OUT / 'Nuvio.app/Contents/MacOS/Nuvio'


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT, stdout=subprocess.DEVNULL)


def wait_for(predicate, timeout=40):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.2)
    raise RuntimeError('Timed out waiting for desktop proof')


def key(value):
    Path('/tmp/nuvio-key').write_text(value + '\n')
    time.sleep(1)


def photo(name):
    source = Path('/tmp/nuvio-shot.bmp')
    source.unlink(missing_ok=True)
    Path('/tmp/nuvio-shot-req').write_text('1\n')
    wait_for(source.exists)
    dest = OUT / name
    run('magick', str(source), '-resize', '1280x800>', str(dest))
    return dest


DATA.mkdir(parents=True, exist_ok=True)
ART.parent.mkdir(parents=True, exist_ok=True)
if not ART.exists():
    shutil.copytree(OUT / 'Nuvio.app/Contents/Resources/art', ART)
fonts = ART.parent / 'fonts'
if not fonts.exists():
    shutil.copytree(ROOT / 'deploy/app/fonts', fonts)
(DATA / 'ajustes.txt').write_text('idioma 1\nidiomaAutoLocal 0\nhomeLayoutLocal 2\ntrailerHero 1\n')
fixture = OUT / 'desktop-scene'
run('cc', '-std=c11', '-Isrc', 'tools/desktop-scene.c', '-o', str(fixture))
run(str(fixture), str(DATA / 'catalogo-rede.bin'), str(ART), str(MOCK))
for path in ('/tmp/nuvio-video', '/tmp/nuvio-key', '/tmp/nuvio-shot-req'):
    Path(path).unlink(missing_ok=True)
log = OUT / 'mac-scene.log'
env = dict(os.environ, NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1')
with log.open('w') as output:
    process = subprocess.Popen([str(APP), str(ART)], env=env, stdout=output,
                               stderr=subprocess.STDOUT, start_new_session=True)
    try:
        wait_for(lambda: 'primeiro quadro na tela' in log.read_text(errors='replace'))
        time.sleep(4)
        key('janela:1280,800')
        photo('mac-home-1280.png')
        key('janela:669,951')
        photo('mac-home-narrow.png')
        key('janela:1280,800')
        clip = OUT / 'sintel-trailer.mp4'
        if clip.exists():
            Path('/tmp/nuvio-video').write_text(str(clip) + '\n')
            time.sleep(4)
            photo('mac-video.png')
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        for path in ('/tmp/nuvio-video', '/tmp/nuvio-key', '/tmp/nuvio-shot-req'):
            Path(path).unlink(missing_ok=True)
# Original mock includes a presentation border. Extract only its app viewport.
run('magick', str(MOCK / 'design/ios-duo/out/12-mac.png'),
    '-crop', '1816x1135+62+127', '+repage', '-resize', '1280x800!', str(OUT / 'mock-1280.png'))
run('magick', str(OUT / 'mock-1280.png'), str(OUT / 'mac-home-1280.png'),
    '+append', str(OUT / 'mock-vs-app.png'))
print('Desktop PNGs:', OUT / 'mock-vs-app.png', OUT / 'mac-home-narrow.png')
