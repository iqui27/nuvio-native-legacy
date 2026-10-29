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
    size = '669x951>' if name == 'mac-home-narrow.png' else '1280x800!'
    run('magick', str(source), '-resize', size, str(dest))
    return dest


if DATA.exists():
    shutil.rmtree(DATA)  # This helper owns only its isolated fixture directory.
DATA.mkdir(parents=True, exist_ok=True)
# Existing tours must not cover the scene under test.
import re
for header in (ROOT / 'src').glob('novidades*.h'):
    for name in re.findall(r'"(novidades[^"/]*\.txt)"', header.read_text()):
        (DATA / name).write_text('1\n')
for name in re.findall(r'\{ "([^"]+\.txt)"', (ROOT / 'src/novidadesfila.c').read_text()) + ['salvos-intro.txt', 'aviso-log.txt', 'recintro-social.txt']:
    (DATA / name).write_text('1\n')
for source in [*(ROOT / 'src').glob('novidades*.c'), *(ROOT / 'src/novidades').glob('*.inc')]:

    for name in re.findall(r'"(novidades[^"/]*\.txt)"', source.read_text()):
        (DATA / name).write_text('1\n')
(DATA / 'dicas-controle-205.txt').write_text('2.1\n')
(DATA / 'diagnostico-otimizacao-intro.cfg').write_text('versao=1\n')
ART.parent.mkdir(parents=True, exist_ok=True)
if not ART.exists():
    shutil.copytree(OUT / 'Nuvio.app/Contents/Resources/art', ART)
shutil.copy2(MOCK / 'design/glass-ilha/arte.jpg', ART / '05.jpg')
(ART / 'addons.txt').write_text('Offline fixture\thttps://example.invalid/manifest.json\t0\t0\t0\n')
fonts = ART.parent / 'fonts'
if not fonts.exists():
    shutil.copytree(ROOT / 'deploy/app/fonts', fonts)
(DATA / 'ajustes.txt').write_text('idioma 1\nidiomaAutoLocal 0\nhomeLayoutLocal 2\ntrailerHero 1\nsocialLocal 1\nheroDifferentFromCard 0\nheroFundoLocal 1\nselected_theme 6\n')
(DATA / 'fileirasui-p1.txt').write_text('limite 7\nordem 1\nmigracao 197\nhero continue_watching\nlinha social_activity\t1\t0\t1\t0\tFriends watching\nlinha continue_watching\t0\t0\t1\t0\tContinue watching\nlinha desktop_trending\t0\t1\t1\t0\tTrending now\n')
(DATA / 'ajustes-p1.txt').write_text('socialLocal 1\nheroDifferentFromCard 0\nheroFundoLocal 1\nselected_theme 6\n')
fixture = OUT / 'desktop-scene'
run('cc', '-std=c11', '-Isrc', 'tools/desktop-scene.c', '-o', str(fixture))
run(str(fixture), str(DATA / 'catalogo-rede.bin'), str(ART), str(MOCK))
for path in ('/tmp/nuvio-video', '/tmp/nuvio-key', '/tmp/nuvio-shot-req'):
    Path(path).unlink(missing_ok=True)
log = OUT / 'mac-scene.log'
env = dict(os.environ, NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NUVIO_MPV_DIAG='1')
with log.open('w') as output:
    process = subprocess.Popen([str(APP), str(ART)], env=env, stdout=output,
                               stderr=subprocess.STDOUT, start_new_session=True)
    try:
        wait_for(lambda: 'primeiro quadro na tela' in log.read_text(errors='replace'))
        time.sleep(1)
        key('right')
        key('left')
        key('janela:1280,800')
        photo('mac-home-1280.png')
        key('mover:1100,25')
        key('clicar:1100,25')
        time.sleep(3)
        photo('mac-search.png')
        assert 'tela=busca' in log.read_text(errors='replace'), 'Toolbar search click failed'
        key('mover:75,114')
        key('clicar:75,114')
        key('janela:669,951')
        photo('mac-home-narrow.png')
        key('janela:1280,800')
        clip = OUT / 'sintel-trailer.mp4'
        if clip.exists():
            Path('/tmp/nuvio-video').write_text(str(clip) + '\n')
            time.sleep(12)
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
