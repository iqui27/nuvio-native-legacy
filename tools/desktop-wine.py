#!/usr/bin/env python3
"""Attempt packaged Windows playback in an existing CrossOver bottle."""
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('NUVIO_DESKTOP_OUT', '/Volumes/ExternalSSD/tmp/rev/desktop'))
WINE = '/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine'
DATA = OUT / 'wine-scene-data'
DATA.mkdir(parents=True, exist_ok=True)
# No user account is read. Windows has its own isolated preference directory.
(DATA / 'ajustes.txt').write_text('idioma 1\nidiomaAutoLocal 0\nhomeLayoutLocal 2\n')
for name in ('/tmp/nuvio-video', '/tmp/nuvio-key', '/tmp/nuvio-shot-req'):
    Path(name).unlink(missing_ok=True)
log = OUT / 'windows-video.log'
env = dict(os.environ, NUVIO_DADOS='Z:' + str(DATA), NUVIO_UI_PREVIEW='1', NUVIO_LOG='Z:' + str(log),
           WINEDEBUG='-all')
with log.open('w') as output:
    process = subprocess.Popen([WINE, '--bottle', 'Windwos apps',
                                str(OUT / 'Nuvio-win64/Nuvio.exe')],
                               cwd=OUT / 'Nuvio-win64', env=env,
                               stdout=output, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        deadline = time.monotonic() + 50
        while time.monotonic() < deadline:
            if 'primeiro quadro na tela' in log.read_text(errors='replace'):
                break
            if process.poll() is not None:
                break
            time.sleep(.3)
        text = log.read_text(errors='replace')
        if 'primeiro quadro na tela' not in text:
            raise RuntimeError('Wine could not start the Windows OpenGL app; see windows-video.log')
        Path('/tmp/nuvio-video').write_text('Z:' + str(OUT / 'sintel-trailer.mp4') + '\n')
        time.sleep(25)
        bmp = Path('/tmp/nuvio-shot.bmp')
        bmp.unlink(missing_ok=True)
        Path('/tmp/nuvio-shot-req').write_text('1\n')
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline and not bmp.exists():
            time.sleep(.2)
        if not bmp.exists():
            raise RuntimeError('Wine screenshot port unavailable; see windows-video.log')
        subprocess.run(['magick', str(bmp), '-resize', '1280x800>',
                        str(OUT / 'windows-video.png')], check=True)
        print('Windows Wine capture:', OUT / 'windows-video.png')
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        # CrossOver launcher may exit while its Windows child stays alive.
        listing = subprocess.run(['pgrep', '-f', str(OUT / 'Nuvio-win64/Nuvio.exe').replace('/', '\\\\')],
                                 text=True, stdout=subprocess.PIPE)
        for pid in listing.stdout.split():
            try:
                os.kill(int(pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
        for name in ('/tmp/nuvio-video', '/tmp/nuvio-key', '/tmp/nuvio-shot-req'):
            Path(name).unlink(missing_ok=True)
