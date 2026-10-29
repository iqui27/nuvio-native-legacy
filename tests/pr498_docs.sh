#!/bin/bash
# Regression: PR #498 must preserve installation, support, and build guidance.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re

readme = Path('README.md').read_text()
for section in ('Install', 'Support development', 'Reporting a problem', 'Building'):
    assert f'## {section}\n' in readme, f'Missing upstream guidance: {section}'
assert 'https://github.com/blob/' not in readme, 'Broken upstream README URL'
for target in ('INSTALL.md', 'docs/plans/microfone-busca.md'):
    assert re.search(r'\]\(' + re.escape(target) + r'\)', readme), f'Missing guide link: {target}'
    assert Path(target).is_file(), f'Broken guide link: {target}'
for target in ('tools/tpk.sh', 'tools/tizen-wgt.sh'):
    assert target in readme and Path(target).is_file(), f'Missing build instructions: {target}'
assert Path('.github/workflows/build-tpk.yml').is_file(), 'TPK build workflow removed'
print('PR #498 documentation: ok')
PY
