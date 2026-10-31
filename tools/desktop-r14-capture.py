#!/usr/bin/env python3
"""r14 native sidebar restyle proof: expanded, hover and selected rows (before/after).
Never activates the app: open -g -j, accessory policy, ordered-back window,
screencapture -l <window id>, commands through the app's own IPC file."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'r14'
SOURCE = Path(os.environ.get('NV_R14_APP', BASE / 'Nuvio.app'))
TAG = os.environ.get('NV_R14_TAG', 'after')
RUNTIME = Path(tempfile.mkdtemp(prefix='nuvio-desktop-r14-', dir='/private/tmp'))
OUT.mkdir(parents=True, exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
subprocess.run(['ditto', str(SOURCE), str(APP)], check=True)
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
LOG = OUT / f'runtime-{TAG}.log'
executable = APP / 'Contents/MacOS/Nuvio'
binary = executable.with_name('Nuvio-proof-bin')
executable.replace(binary)
executable.write_text('#!/bin/bash\nexec "' + str(binary) + '" "$@" > "' + str(LOG) + '" 2>&1\n')
executable.chmod(0o755)
subprocess.run(['codesign', '--force', '--sign', '-', str(APP)], check=True, capture_output=True)
source = RUNTIME / 'helper.m'
source.write_text('''#import <Cocoa/Cocoa.h>
int main(int argc, char **argv) { @autoreleasepool {
  if (argc < 2) { printf("%d\\n",NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier); return 0; }
  // Windows of a pid, any Space: id x y w h (macOS 27 hosts NSAlert sheets in their own window).
  NSArray *list = CFBridgingRelease(CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID));
  for (NSDictionary *d in list) if ([d[@"kCGWindowOwnerPID"] intValue] == atoi(argv[1]) && [d[@"kCGWindowLayer"] intValue] == 0 && [d[@"kCGWindowAlpha"] floatValue] > 0) {
    CGRect r; CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)d[@"kCGWindowBounds"], &r);
    printf("%d %.0f %.0f %.0f %.0f\\n", [d[@"kCGWindowNumber"] intValue], r.origin.x, r.origin.y, r.size.width, r.size.height);
  }
} }
''')
helper = RUNTIME / 'helper'
subprocess.run(['cc', '-fobjc-arc', str(source), '-framework', 'Cocoa', '-o', str(helper)], check=True)
def front():
    return int(subprocess.check_output([str(helper)]))
def windows(pid):
    rows = subprocess.check_output([str(helper), str(pid)], text=True).split('\n')
    return {int(r.split()[0]): [int(v) for v in r.split()[1:]] for r in rows if r.strip()}
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
def window_shot(window, path):
    subprocess.run(['screencapture', '-x', '-o', '-l', str(window), str(path)], check=True)
    assert path.stat().st_size > 10000, path
def shot(name, window):
    dest = OUT / (name + '.png')
    window_shot(window, dest)
    print(dest, flush=True)
    return str(dest)
def sheet_shot(name, window, pid):
    deadline = time.monotonic() + 20
    while '[desktop-sheet]' not in LOG.read_text(errors='replace'):
        if time.monotonic() > deadline:
            raise RuntimeError('No native sheet')
        time.sleep(.2)
    time.sleep(2)  # sheet slide-in animation
    found = windows(pid)
    px, py, pw, ph = found.pop(window)
    sheet, (sx, sy, sw, sh) = next((k, v) for k, v in found.items()
                                   if px <= v[0] and v[0] + v[2] <= px + pw and v[1] >= py)
    parent_png, sheet_png, dest = RUNTIME / 'parent.png', RUNTIME / 'sheet.png', OUT / (name + '.png')
    window_shot(window, parent_png); window_shot(sheet, sheet_png)
    scale = int(subprocess.check_output(['magick', 'identify', '-format', '%w', str(parent_png)], text=True)) / pw
    # macOS 27 captures a sheet together with its parent window: use it as is.
    if int(subprocess.check_output(['magick', 'identify', '-format', '%w', str(sheet_png)], text=True)) >= pw * scale:
        subprocess.run(['cp', str(sheet_png), str(dest)], check=True)
    else: subprocess.run(['magick', str(parent_png), str(sheet_png), '-geometry',
                    f'+{round((sx - px) * scale)}+{round((sy - py) * scale)}', '-composite', str(dest)], check=True)
    print(dest, flush=True)
    return dict(file=str(dest), sheet=sheet, sheet_bounds=[sx, sy, sw, sh], parent_bounds=[px, py, pw, ph])
environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
                   NV_CAPTURA_DIR=str(ipc), SDL_MAC_BACKGROUND_APP='1',
                   SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
def session(steps):
    LOG.write_text('')
    launch = ['open', '-n', '-g', '-j']
    for key, value in environment.items():
        launch += ['--env', key + '=' + value]
    launch += ['-a', str(APP), '--args', str(ART)]
    pid = None
    try:
        subprocess.run(launch, check=True)
        deadline = time.monotonic() + 60
        while 'primeiro quadro na tela' not in LOG.read_text(errors='replace'):
            if time.monotonic() > deadline:
                raise RuntimeError('Background launch timeout')
            time.sleep(.2)
        pid = int(subprocess.check_output(['pgrep', '-f', '^' + re.escape(str(binary))], text=True).splitlines()[-1])
        text = LOG.read_text(errors='replace')
        window = int(re.search(r'\[desktop-native\] window=(\d+)', text).group(1))
        assert '[desktop-momentum] sdl=0 now=1' in text, 'SDL momentum default not restored'
        assert front() != pid
        command('appearance:DarkAqua')
        command('back')
        command('janela:1440,900', 2)
        result = steps(window, pid)
        assert front() != pid
        return result, text
    finally:
        if pid:
            try:
                os.kill(pid, signal.SIGTERM)
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    try: os.kill(pid, 0)
                    except ProcessLookupError: break
                    time.sleep(.05)
                else: os.kill(pid, signal.SIGKILL)
            except ProcessLookupError: pass
def sidebar(window, pid):
    command('preview:home', 3)
    name = 'expanded-hover' if environment.get('NV_CAPTURA_HOVER') else 'expanded'
    return [shot(f'sidebar-{TAG}-{name}', window)]
def library(window, pid):
    command('preview:library', 3)
    return [shot(f'sidebar-{TAG}-selected-library', window)]
shots = []
try:
    shots += session(sidebar)[0]
    shots += session(library)[0]
    environment['NV_CAPTURA_HOVER'] = '5'  # Library row hovered while Home is the page
    shots += session(sidebar)[0]
finally:
    (OUT / f'focus-{TAG}.json').write_text(json.dumps(dict(before=foreground, after=front())))
    (OUT / f'captures-{TAG}.json').write_text(json.dumps(dict(shots=shots), indent=2))
assert subprocess.run(['pgrep', '-f', '^' + re.escape(str(binary))], capture_output=True).returncode == 1
print('PASS: r14 sidebar captured; proof app never foreground; stopped')
