#!/usr/bin/env python3
"""Verify exported r6 PNG dimensions and the sidebar-only GL alpha hole."""
import json
from pathlib import Path
import struct
import subprocess

out = Path('/Volumes/ExternalSSD/tmp/rev/desktop/r6')
for stage in ('before', 'after'):
    for scene in ('home', 'detail-top', 'detail-scrolled', 'detail-lower', 'native-glass'):
        path = out / (f'{stage}-{scene}.png' if scene == 'native-glass'
                      else f'{stage}-{scene}-1920x1080.png')
        png = path.read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n', path
        assert struct.unpack('>II', png[16:24]) == (1920, 1080), path
        if scene != 'native-glass':
            data = json.loads(path.with_suffix('.json').read_text())
            assert data['sidebar_alpha'] == 0, path
            assert data['content_alpha'] == 255, path
    assert 'glass=1' in (out / f'{stage}-runtime.log').read_text()
result = subprocess.run(['pgrep', '-f', '^/private/tmp/nuvio-desktop-r6/Nuvio.app/Contents/MacOS/Nuvio-proof-bin'], capture_output=True)
assert result.returncode == 1, 'Proof instance still running'
print('r6: 10 PNGs at 1920x1080; sidebar alpha 0, content alpha 255; native glass; no proof instance')
