#!/bin/bash
# API11's GLView and its owning window must both keep rendering over video.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
import subprocess

source = Path('tizen-tpk/Program.cs').read_text()
for symbol in ('NV_API8', 'NV_API9', 'NV_API11'):
    active = subprocess.run(
        ['clang', '-E', '-P', '-x', 'c', f'-D{symbol}', '-'],
        input=source, text=True, capture_output=True, check=True).stdout
    if symbol != 'NV_API11':
        assert 'RenderingBehaviorType.Continuously' not in active, symbol
        continue
    method = active.split('void CriaGlView()', 1)[1].split('IntPtr glEcore', 1)[0]
    assert re.search(
        r'NuiWindow\.Instance\.RenderingBehavior\s*=\s*RenderingBehaviorType\.Continuously\s*;',
        method), 'API11: static video hole must not leave the owning window on IfRequired'
    assert method.index('RenderingBehaviorType.Continuously') < method.index('NuiWindow.Instance.Add(glView)')
    assert 'glView.RenderingMode = GLRenderingMode.Continuous;' in method
    assert 'GLView.ColorFormat.RGBA8888' in method
    assert 'BackgroundColor = NuiColor.Transparent' in method
    assert '() => Quadro()' in method
    assert 'SetGraphicsConfig(false, false, 0, GLESVersion.Version20)' in method
print('PASS: API11 window and GLView render continuously; alpha/callback preserved; API8/9 unchanged')
PY
