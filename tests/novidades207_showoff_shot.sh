#!/bin/bash
# Focused 2.0.7 visual regression: all languages, scales, frost and D-pad.
set -euo pipefail
cd "$(dirname "$0")/.."
# The static scanner does not follow release .inc files; check their keys here.
python3 - <<'PY'
import ast
import re
from pathlib import Path

pair = re.compile(r'(?:\{ |T\()("(?:[^"\\]|\\.)*"), ("(?:[^"\\]|\\.)*")')
master = dict((ast.literal_eval(m[1]), ast.literal_eval(m[2]))
              for m in pair.finditer(Path('src/idioma_tab.h').read_text()))
items = Path('src/novidades/207.inc').read_text().split('static const NovCena')[0]
keys = re.findall(r'\.(?:grupo|nome|frase) = "([^"]*)"', items)
assert all(key in master for key in keys), 'Untranslated release copy'
for path in Path('src').glob('idioma_*.h'):
    entries = [(ast.literal_eval(m[1]), ast.literal_eval(m[2]))
               for m in pair.finditer(path.read_text())]
    assert [key for key, _ in entries] == list(master), path
    assert all(dict(entries)[key] for key in keys), path
notes = Path('docs/releases/2.0.7/NOVIDADES.md').read_text()
pt, en = notes.split('## Português\n', 1)[1].split('## English\n')
entries = re.findall(r'\.nome = "([^"]*)",\s*\.frase = "([^"]*)"', items)
for section, translate in [(pt, lambda s: s), (en, master.__getitem__)]:
    expected = [f'- **{translate(name)}.** {translate(text)}' for name, text in entries]
    assert section.count('- **') == len(expected)
    assert [line for line in section.splitlines() if line.startswith('- **')] == expected
assert len(entries) <= 35
print('PASS: release notes/card parity, translated keys and aligned language tables')
PY
export NUVIO_SHOT_SO=showoff NUVIO_SHOT_FONTE=3
bash tests/novidades_cartao_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/novidades207}"
