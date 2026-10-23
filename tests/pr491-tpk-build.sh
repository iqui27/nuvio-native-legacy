#!/bin/bash
# PR #491: CI must build all Tizen 6+ packages and cache the DTS FFmpeg build.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path

path = Path('.github/workflows/build-tpk.yml')
assert path.is_file(), 'missing Tizen 6+ build workflow'
workflow = path.read_text()
packages = 'NuvioTpk60 NuvioTpk65 NuvioTpk'
assert f'default: "{packages}"' in workflow
assert "github.event.inputs.pacotes || '" + packages + "'" in workflow
assert 'bash tools/tpk.sh' in workflow
assert 'path: build/tpk/*.tpk' in workflow
assert 'if-no-files-found: error' in workflow
assert "hashFiles('tools/build-dts-ffmpeg.sh')" in workflow
assert 'prefix-dts/.carimbo' in workflow
assert "steps.dts.outputs.pronto == '1'" in workflow
assert 'secrets.' not in workflow, 'build should not copy repository secrets'
print('PR #491: Tizen 6/6.5/7/8+ build contract OK')
PY
