#!/usr/bin/env python3
"""Capture native menu and row timing without activating the proof app."""
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'r8'
RUNTIME = Path('/private/tmp/nuvio-desktop-r8')
OUT.mkdir(parents=True, exist_ok=True)
RUNTIME.mkdir(exist_ok=True)
APP = RUNTIME / 'Nuvio.app'
subprocess.run(['ditto', str(BASE / 'Nuvio.app'), str(APP)], check=True)
DATA = RUNTIME / 'data'
ART = RUNTIME / 'resources/art'
subprocess.run(['ditto', str(BASE / 'scene-data'), str(DATA)], check=True)
if not ART.parent.exists():
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
# Read-only window inventory and foreground sentinel. Never posts global input.
source = RUNTIME / 'windows.m'
source.write_text('''#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
int main(void) { @autoreleasepool {
 NSArray *windows=(NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionAll,kCGNullWindowID);
 NSDictionary *result=@{@"front":@([NSWorkspace sharedWorkspace].frontmostApplication.processIdentifier),@"windows":windows};
 NSData *data=[NSJSONSerialization dataWithJSONObject:result options:0 error:nil];
 fwrite(data.bytes,1,data.length,stdout); [windows release];
} }
''')
helper = RUNTIME / 'windows'
subprocess.run(['cc', str(source), '-framework', 'Cocoa', '-framework', 'CoreGraphics', '-o', str(helper)], check=True)
def inventory():
    return json.loads(subprocess.check_output([str(helper)]))
front = inventory()['front']
with tempfile.TemporaryDirectory(prefix='desktop-r8-', dir=RUNTIME) as temporary:
    ipc = Path(temporary)
    def command(value, delay=.15):
        staging = ipc / 'pending'
        staging.write_text(value + '\n')
        staging.replace(ipc / 'nuvio-key')
        deadline = time.monotonic() + 45
        while (ipc / 'nuvio-key').stat().st_size:
            if time.monotonic() > deadline:
                raise RuntimeError('Input timed out: ' + value)
            time.sleep(.02)
        time.sleep(delay)
    environment = dict(NUVIO_DADOS=str(DATA), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
                       NV_CAPTURA_MENU='1', NUVIO_DESKTOP_FRAME_LOG='1',
                       NV_CAPTURA_DIR=str(ipc), SDL_MAC_BACKGROUND_APP='1',
                       SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
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
                raise RuntimeError('Background launch timed out')
            time.sleep(.2)
        pid = int(subprocess.check_output(['pgrep', '-f', '^' + re.escape(str(binary))], text=True).splitlines()[-1])
        command('janela:1440,900')
        command('appearance:DarkAqua')
        command('back')
        command('preview:home', 5)
        # Scroll vertically to put the first row under the pointer.
        command('wheel:700,500,0,-620,33')
        command('wheel:700,500,0,0,34', 1)
        command('wheel:700,550,-.5,0,33', .02)
        for i in range(90):
            if i == 45:
                command('wheel:700,550,0,0,34', .012)
            phase = 32 if i < 45 else 40
            delta = -1.25 if i < 45 else -1.25 * math.exp(-(i-45)/12)
            command(f'wheel:700,550,{delta:.4f},0,{phase}', .012)
        command('wheel:700,550,0,0,42', 1)
        command('preview:home', 2)
        command('wheel:700,500,0,-620,33')
        command('wheel:700,500,0,0,34', 2)
        command('direito:700,550', 1)
        deadline = time.monotonic() + 4
        while '[desktop-menu]' not in LOG.read_text(errors='replace'):
            if time.monotonic() > deadline:
                raise RuntimeError('Native context menu did not open')
            time.sleep(.05)
        deadline = time.monotonic()+5
        while True:
            state = inventory()
            assert state['front'] != pid, 'Proof app stole foreground focus'
            windows = [w for w in state['windows'] if w.get('kCGWindowOwnerPID') == pid]
            if any(w.get('kCGWindowLayer',0)>0 and w.get('kCGWindowIsOnscreen') for w in windows):
                break
            if time.monotonic()>deadline:
                raise RuntimeError('Native menu did not become visible')
            time.sleep(.05)
        (OUT / 'native-windows.json').write_text(json.dumps(windows, indent=2))
        main = next(w for w in windows if w['kCGWindowNumber'] == int(re.search(r'\[desktop-native\] window=(\d+)', LOG.read_text()).group(1)))
        subprocess.run(['screencapture', '-x', '-o', '-l', str(main['kCGWindowNumber']), str(OUT / 'native-context-home.png')], check=True)
        # Native menu tracking uses a separate system window. Save it too.
        menu = next(w for w in windows if w.get('kCGWindowLayer', 0)>0 and w.get('kCGWindowIsOnscreen'))
        subprocess.run(['screencapture', '-x', '-o', '-l', str(menu['kCGWindowNumber']), str(OUT / 'native-menu-window.png')], check=True)
        # screencapture -l excludes child windows. Composite only these two
        # app-owned captures at their real global positions; keep both sources.
        from struct import unpack
        main_size = unpack('>II', (OUT / 'native-context-home.png').read_bytes()[16:24])
        menu_size = unpack('>II', (OUT / 'native-menu-window.png').read_bytes()[16:24])
        mb, cb = main['kCGWindowBounds'], menu['kCGWindowBounds']
        scale = main_size[0] / mb['Width']
        shadow_x = (menu_size[0] - cb['Width']*scale)/2
        shadow_y = (menu_size[1] - cb['Height']*scale)/2
        x = round((cb['X']-mb['X'])*scale-shadow_x)
        y = round((cb['Y']-mb['Y'])*scale-shadow_y)
        subprocess.run(['magick', str(OUT / 'native-context-home.png'), str(OUT / 'native-menu-window.png'), '-geometry', f'+{x}+{y}', '-composite', str(OUT / 'native-context-over-home.png')], check=True)
        after=inventory()['front']
        assert after != pid, 'Capture activated proof app'
        (OUT / 'focus.json').write_text(json.dumps(dict(before=front,during=state['front'],after=after,proof_pid=pid)))
        lines = [s for s in LOG.read_text(errors='replace').splitlines() if '[desktop-scroll]' in s]
        assert len(lines) >= 10, 'No row timing sample'
        offsets = [s for s in LOG.read_text(errors='replace').splitlines() if '[desktop-offset] axis=x' in s]
        assert len(offsets)>=10, 'No horizontal row movement'
        (OUT / 'row-offsets.log').write_text('\n'.join(offsets)+'\n')
        (OUT / 'scroll-timing.log').write_text('\n'.join(lines) + '\n')
        print(lines[-1])
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
assert subprocess.run(['pgrep', '-f', '^' + re.escape(str(binary))], capture_output=True).returncode == 1
print('PASS: native menu captured; proof app never foreground; row timing logged; proof instance stopped')
