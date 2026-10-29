#!/usr/bin/env python3
"""Capture the real packaged SDL app, isolated catalog and production routes."""
import os
import sys
import json
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import threading
import tempfile
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
PROOF = OUT / 'polish'
PROOF.mkdir(exist_ok=True)
APP = OUT / 'Nuvio.app/Contents/MacOS/Nuvio'
DATA = OUT / 'scene-data'
LOG = PROOF / 'runtime.log'
CAPTURE = tempfile.TemporaryDirectory(prefix='nuvio-polish-')
IPC = Path(CAPTURE.name)
def launch_background(output, native=False):
    """Never activate the app or manipulate the owner's desktop/input."""
    env = dict(os.environ, NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1',
               NUVIO_DESKTOP_FRAME_LOG='1', NV_CAPTURA='1', NV_CAPTURA_DIR=str(IPC),
               SDL_MAC_BACKGROUND_APP='1', SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
    if not native:
        env['NV_CAPTURA_GL'] = '1'
    return subprocess.Popen([str(APP), str(OUT / 'scene-resources/art')],
                            env=env, stdout=output, stderr=subprocess.STDOUT)


def native_photo(appearance):
    command('appearance:' + appearance, 2)
    windows = re.findall(r'\[desktop-native\] window=(\d+)', LOG.read_text(errors='replace'))
    if not windows:
        raise RuntimeError('Native window ID missing')
    subprocess.run(['screencapture', '-l', windows[-1], '-o', '-x',
                    str(PROOF / f'native-{appearance}.png')], check=True)


def command(value, delay=.7):
    pending=IPC / 'nuvio-key'
    staging=IPC / 'nuvio-key-polish'
    staging.write_text(value.replace("edge-leave:","mover:") + '\n')
    staging.replace(pending)
    deadline=time.monotonic()+10
    while pending.exists() and pending.stat().st_size and time.monotonic()<deadline:
        time.sleep(.02)
    if pending.exists() and pending.stat().st_size: raise RuntimeError('Command not consumed: '+value)
    time.sleep(delay)


def photo(name, width, height):
    dest = PROOF / f'{name}-{width}x{height}.png'
    source = IPC / 'nuvio-shot.bmp'
    source.unlink(missing_ok=True)
    (IPC / 'nuvio-shot-req').write_text('1\n')
    deadline = time.monotonic() + 30
    retry = time.monotonic() + 8
    while time.monotonic() < deadline:
        if source.exists():
            with source.open('rb') as bitmap:
                header = bitmap.read(6)
            if len(header) == 6 and source.stat().st_size == int.from_bytes(header[2:6], 'little'):
                break
        if time.monotonic() > retry:
            (IPC / 'nuvio-shot-req').write_text('1\n')
            retry=time.monotonic()+8
        time.sleep(.1)
    else:
        raise RuntimeError('Screenshot incomplete or timed out: ' + name)
    subprocess.run(['magick', str(source), '-resize', f'{width}x{height}!', str(dest)], check=True)
    return dest


class Catalog(BaseHTTPRequestHandler):
    def do_GET(self):
        titles=["Dune", "Interstellar", "Inception", "The Dark Knight", "Dunkirk", "Tenet", "Oppenheimer", "The Prestige"]
        body=json.dumps({"id":"desktop-proof","name":"Offline catalog","catalogs":[{"id":"films","type":"movie","name":"Films"}],"resources":["catalog"],"types":["movie"]} if "manifest" in self.path else {"metas":[{"id":f"tt{1000000+i}","type":"movie","name":title,"year":2000+i,"genres":["Drama"],"description":"Offline proof fixture. A story of discovery, choices and consequences.","poster":str(OUT / f"scene-resources/art/poster/{i:02}.jpg"),"background":str(OUT / f"scene-resources/art/{i:02}.jpg")} for i,title in enumerate(titles)]}).encode()
        self.send_response(200);self.send_header("Content-Type","application/json");self.end_headers();self.wfile.write(body)
    def log_message(self,*args):
        pass
server=ThreadingHTTPServer(("127.0.0.1",0),Catalog)
threading.Thread(target=server.serve_forever,daemon=True).start()
base=f"http://127.0.0.1:{server.server_port}"
source={"title":"Films","base":base,"type":"movie","catId":"films"}
folders=[{"id":"nolan","title":"Christopher Nolan","sources":[source]}]
services=[{"id":name.lower().replace(" ","-"),"title":name,"sources":[source]} for name in ("Netflix","Prime Video","Disney+","Apple TV+")]
(OUT / "scene-resources/art/collections.json").write_text(json.dumps({"groups":[{"id":"directors","title":"Directors","folders":folders},{"id":"streaming","title":"Streaming","folders":services}]}))
(OUT / "scene-resources/art/addons.txt").write_text(f"Offline catalog\t{base}/manifest.json\t0\t0\t0\n")
with tempfile.TemporaryDirectory(prefix='nuvio-catalog-') as work:
    fixture=Path(work)/'catalog'
    subprocess.run(['cc','-Isrc','tools/desktop-scene.c','-o',str(fixture)],cwd=ROOT,check=True)
    subprocess.run([str(fixture),str(DATA/'catalogo-rede.bin'),str(OUT/'scene-resources/art'),str(ROOT)],check=True)
(DATA / 'fileirasui-p1.txt').write_text('limite 7\nordem 1\nmigracao 197\nhero continue_watching\n'
    'linha continue_watching\t0\t0\t1\t0\tContinue watching\n'
    'linha desktop_trending\t0\t1\t1\t0\tTrending now\n'
    'linha desktop_more_2\t0\t2\t1\t0\tPopular series\n'
    'linha desktop_more_3\t0\t3\t1\t0\tMore to explore\n'
    'linha collection_streaming\t0\t4\t1\t0\tStreaming\n'
    'linha collection_directors\t0\t5\t1\t0\tDirectors\n')
(DATA / 'desktop-sidebar.txt').write_text('0\n')
(DATA / 'perfis.txt').write_text('1\t0\t1\t0\t#777B83\tDesktop Viewer\t\t\t0\n')
if '--native-only' not in sys.argv:
    with LOG.open('w') as output:
        process = launch_background(output)
        try:
            deadline = time.monotonic() + 35
            while 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('App failed startup')
                time.sleep(.2)
            time.sleep(1)
            for width, height in ((1280, 800), (1920, 1080)):
                command(f'janela:{width},{height}')
                command('preview:home',2)
                command('preview:expand')
                photo('home', width, height)
                photo('sidebar-expanded', width, height)
                photo('hero-scroll-0', width, height)
                for step in (50,100):
                    command(f'wheel:{width//2},{height//2},0,{-height/2}',1.2)
                    photo(f'hero-scroll-{step}',width,height)
                command('preview:home',2)
                command(f'wheel:{width//2},{height//2},0,{-height*.3}',1.2)
                command(f'mover:{width//3},{int(height*.8)}')
                photo('continue-hover', width, height)
                command(f'direito:{int(width*.28)},{int(height*.8)}',1.2)
                photo('continue-menu', width, height)
                photo('context-menu',width,height)
                command('back',1.2)
                command('preview:collapse', 3)
                command(f'mover:{width//2},{height//2}',1.2)
                photo('sidebar-collapsed', width, height)
                command('mover:2,320',1.2)
                photo('sidebar-edge-reveal',width,height)
                command(f'edge-leave:{width//2},{height//2}',1.2)
                command('preview:expand', 1.2)
                command('preview:streaming')
                photo('streaming', width, height)
                command('preview:streaming')
                command('preview:search')
                command('input:Fallout é')
                photo('search-typing', width, height)
                for screen in ('library', 'lists', 'settings', 'guide', 'sports', 'achievements'):
                    command('preview:home')
                    command('preview:' + screen, 1.2)
                    photo(screen, width, height)
                    if screen == 'library':
                        command(f'clicar:{int(width*.88)},{int(height*.12)}')
                        photo('library-popup', width, height)
                command('preview:home')
                command('abrir:tt12637874', 3)
                photo('detail', width, height)
                command(f'wheel:{width//2},{height*3//4},0,{-height*.65}',1.2)
                photo('detail-scrolled',width,height)
                command('preview:library')
                command('abrir:tt12637874', 2)
                photo('detail-from-library', width, height)
                command('preview:filmography', 2.5)
                photo('filmography', width, height)
                command(f"wheel:{width//2},{height*3//4},0,-180",.5)
                photo('filmography-scrolled', width, height)
                command('preview:home')
                for i in range(45):
                    command(f'wheel:{width//2},{height*3//4},0,-12', .009)
                time.sleep(.5)
                photo('scroll', width, height)
            command('preview:home')
            command('janela:1280,800')
            command('preview:collapse',1)
            frames=PROOF / 'sidebar-frames'
            frames.mkdir(exist_ok=True)
            if '--reuse-transitions' not in sys.argv and not os.environ.get('NV_CAPTURE_REUSE_TRANSITIONS'):
                for cycle in range(20):
                    for phase,event in (('open','mover:2,320'),('close','mover:900,320')):
                        command(event,.025)
                        for frame in range(3):
                            path=photo(f'sidebar-{cycle:02}-{phase}-{frame}',1280,800)
                            path.replace(frames / path.name)
            command('preview:expand',1.2)
            command('abrir:tt12637874',2)
            clip = OUT / 'sintel-trailer.mp4'
            if clip.exists():
                command("janela:1280,800")
                (IPC / 'nuvio-video').write_text(str(clip) + '\n')
                time.sleep(5)
                for width,height in ((1280,800),(1920,1080)):
                    command(f"janela:{width},{height}")
                    command(f"mover:{width//2},{height//2}")
                    photo('player-overlay', width, height)
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            for name in ('nuvio-key', 'nuvio-video', 'nuvio-shot-req'):
                (IPC / name).unlink(missing_ok=True)
if '--gl-only' not in sys.argv:
    with LOG.open('a') as output:
        process = launch_background(output, native=True)
        try:
            time.sleep(8)
            command('preview:home')
            command('janela:1280,800')
            command('preview:collapse',1)
            frames=PROOF / 'native-sidebar-frames'
            frames.mkdir(exist_ok=True)
            if '--reuse-transitions' not in sys.argv and not os.environ.get('NV_CAPTURE_REUSE_TRANSITIONS'):
                for cycle in range(20):
                    for phase,event in (('open','mover:2,320'),('close','mover:900,320')):
                        command(event,.025)
                        for frame in range(3):
                            path=photo(f'native-sidebar-{cycle:02}-{phase}-{frame}',1280,800)
                            path.replace(frames / path.name)
            command('preview:expand')
            command('abrir:tt12637874', 3)
            for appearance in ('Aqua', 'DarkAqua'):
                native_photo(appearance)
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
lines = [line for line in LOG.read_text(errors='replace').splitlines() if '[desktop-scroll]' in line]
(PROOF / 'frame-times.log').write_text('\n'.join(lines) + '\n')
for size in ('1280x800', '1920x1080'):
    for name, mock in (('home', OUT / 'mock-1280.png'), ('sidebar-expanded', ROOT.parent / 'nv-21-21-iosmock/design/ios-duo/out/13-sidebar.png'), ('streaming', ROOT.parent / 'nv-21-21-iosmock/design/ios-duo/out/14-sidebar-options.png')):
        subprocess.run(['magick', str(mock), '-resize', size + '!', str(PROOF / f'{name}-{size}.png'), '+append', str(PROOF / f'compare-{name}-{size}.png')], check=True)
print('Proof:', PROOF)
