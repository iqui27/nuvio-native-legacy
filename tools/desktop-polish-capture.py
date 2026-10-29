#!/usr/bin/env python3
"""Capture the real packaged SDL app, isolated catalog and production routes."""
import os
import json
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import threading
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


def command(value, delay=.7):
    Path('/tmp/nuvio-key').write_text(value + '\n')
    time.sleep(delay)


def photo(name, width, height):
    dest = PROOF / f'{name}-{width}x{height}.png'
    window=re.search(r'\[desktop-native\] window=(\d+)',LOG.read_text(errors="replace"))
    if window:
        result=subprocess.run(["screencapture","-x","-o","-l",window.group(1),str(dest)])
        if result.returncode == 0:
            subprocess.run(["magick",str(dest),"-resize",f"{width}x{height}!",str(dest)],check=True)
            return dest
    source = Path('/tmp/nuvio-shot.bmp')
    source.unlink(missing_ok=True)
    Path('/tmp/nuvio-shot-req').write_text('1\n')
    deadline = time.monotonic() + 15
    while not source.exists() and time.monotonic() < deadline:
        time.sleep(.1)
    if not source.exists():
        raise RuntimeError('Screenshot timed out: ' + name)
    time.sleep(.15)
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
with (DATA / "fileirasui-p1.txt").open("a") as rows:
    rows.write("linha collection_streaming\t0\t2\t1\t0\tStreaming\nlinha collection_directors\t0\t3\t1\t0\tDirectors\n")
(DATA / 'desktop-sidebar.txt').write_text('0\n')
for path in ('/tmp/nuvio-key', '/tmp/nuvio-video', '/tmp/nuvio-shot-req'):
    Path(path).unlink(missing_ok=True)
env = dict(os.environ, NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NUVIO_DESKTOP_FRAME_LOG='1')
original_appearance=subprocess.check_output(["osascript","-e",'tell application "System Events" to tell appearance preferences to get dark mode'],text=True,timeout=10).strip()
with LOG.open('w') as output:
    process = subprocess.Popen([str(APP), str(OUT / 'scene-resources/art')], env=env,
                               stdout=output, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 35
        while 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
            if process.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('App failed startup')
            time.sleep(.2)
        time.sleep(1)
        for width, height in ((1280, 800), (1920, 1080)):
            command(f'janela:{width},{height}')
            command('preview:home')
            command('preview:expand')
            photo('home', width, height)
            photo('sidebar-expanded', width, height)
            command('preview:collapse', 1.2)
            photo('sidebar-collapsed', width, height)
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
            command('preview:home')
            command('abrir:tt12637874', 3)
            photo('detail', width, height)
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
        command('preview:expand',1.2)
        window = re.search(r'\[desktop-native\] window=(\d+)', LOG.read_text(errors='replace'))
        if window:
            windowid = window.group(1)
            for appearance in ('Aqua', 'DarkAqua'):
                subprocess.run(['osascript', '-e', f'tell application "System Events" to tell appearance preferences to set dark mode to {"true" if appearance == "DarkAqua" else "false"}'], check=False, timeout=10)
                time.sleep(1.5)
                subprocess.run(['screencapture', '-x', '-l', windowid, str(PROOF / f'native-{appearance}.png')], check=True)
        clip = OUT / 'sintel-trailer.mp4'
        if clip.exists():
            command("janela:1280,800")
            Path('/tmp/nuvio-video').write_text(str(clip) + '\n')
            time.sleep(5)
            for width,height in ((1280,800),(1920,1080)):
                command(f"janela:{width},{height}")
                command(f"mover:{width//2},{height//2}")
                photo('player-overlay', width, height)
    finally:
        subprocess.run(["osascript","-e",f'tell application "System Events" to tell appearance preferences to set dark mode to {original_appearance}'],check=False,timeout=10)
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        for path in ('/tmp/nuvio-key', '/tmp/nuvio-video', '/tmp/nuvio-shot-req'):
            Path(path).unlink(missing_ok=True)
lines = [line for line in LOG.read_text(errors='replace').splitlines() if '[desktop-scroll]' in line]
(PROOF / 'frame-times.log').write_text('\n'.join(lines) + '\n')
for size in ('1280x800', '1920x1080'):
    for name, mock in (('home', OUT / 'mock-1280.png'), ('sidebar-expanded', ROOT.parent / 'nv-21-21-iosmock/design/ios-duo/out/13-sidebar.png'), ('streaming', ROOT.parent / 'nv-21-21-iosmock/design/ios-duo/out/14-sidebar-options.png')):
        subprocess.run(['magick', str(mock), '-resize', size + '!', str(PROOF / f'{name}-{size}.png'), '+append', str(PROOF / f'compare-{name}-{size}.png')], check=True)
print('Proof:', PROOF)
