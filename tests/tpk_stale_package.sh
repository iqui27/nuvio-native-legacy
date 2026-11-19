#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path

source = Path('tools/tpk.sh').read_text()
cleanup = 'find "$H/bin/Release" -type f -name \'*.tpk\' -delete'
build = 'dotnet build "$H/$p.csproj"'
assert cleanup in source, 'tpk.sh does not remove stale host packages'
assert source.index(cleanup) < source.index(build), 'stale package cleanup runs after dotnet build'
print('ok: stale host packages are removed before dotnet build')
PY
