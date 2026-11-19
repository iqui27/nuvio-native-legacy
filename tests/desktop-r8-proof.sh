#!/bin/bash
# Verify recorded AppKit proof and timing; never launches or activates an app.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import json
from pathlib import Path
import re
import struct
import subprocess
out=Path('/Volumes/ExternalSSD/tmp/rev/desktop/r8')
for name in ('native-context-home.png','native-menu-window.png','native-context-over-home.png'):
    data=(out/name).read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n'
    w,h=struct.unpack('>II',data[16:24])
    assert w>100 and h>100
windows=json.loads((out/'native-windows.json').read_text())
assert any(w.get('kCGWindowLayer',0)>0 and w.get('kCGWindowIsOnscreen') for w in windows)
assert '[desktop-menu] native=NSMenu' in (out/'runtime.log').read_text()
offsets=(out/'row-offsets.log').read_text().splitlines()
assert len(offsets)>=60, 'Expected finger and momentum row events'
deltas=[float(re.search(r'delta=([-\d.]+)',s)[1]) for s in offsets]
assert any(0<abs(d)<.1 for d in deltas), 'Missing fractional momentum tail'
frames=(out/'scroll-timing.log').read_text().splitlines()
assert len(frames)>20 and 'dropped=' in frames[-1]
assert subprocess.run(['pgrep','-f','^/private/tmp/nuvio-desktop-r8/Nuvio.app/Contents/MacOS/Nuvio-proof-bin'],capture_output=True).returncode==1
native=Path('src/desktop_mac.m').read_text()
assert 'popUpMenuPositioningItem:nil' in native
assert 'NSMenuItemImageVisibilityVisible' in native
capture=Path('tools/desktop-r8-capture.py').read_text()
assert "['open', '-n', '-g', '-j']" in capture
assert "state['front'] != pid" in capture
focus=json.loads((out/'focus.json').read_text())
assert all(focus[k]!=focus['proof_pid'] for k in ('before','during','after'))
assert 'CGEventPost' not in capture and 'osascript' not in capture
for name in ('build','polish','package'):
    assert (out/(name+'.rc')).read_text().strip()=='0'
print('PASS: native menu PNG sources/composite, finger + momentum row offsets, timing/drop count, background-only capture and stopped instance')
print(frames[-1])
PY
git diff --check
