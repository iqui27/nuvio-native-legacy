#!/bin/bash
# Focused desktop behavior and portability gate; no full application builds.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/desktop_ui.sh
bash tests/desktop_ui_syntax.sh
bash tests/desktop.sh
bash tests/video_desktop_mpv.sh
python3 - <<'PY'
import ast
from pathlib import Path
for name in ('desktop-capture.py', 'desktop-wine.py', 'desktop-proof.py', 'mac-mpv-bundle.py'):
    path = Path('tools') / name
    ast.parse(path.read_text(), filename=str(path))
print('Desktop proof helpers: syntax OK')
PY
git diff --check
