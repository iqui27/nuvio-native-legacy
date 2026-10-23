#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-skip-loading.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
def function(signature):
    start = s.index(signature)
    body = s.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
parts = []
if 'static int skipDisponivel(void) {' in s:
    parts.append(function('static int skipDisponivel(void) {'))
parts.append(function('static int trechoPulavel(double *fim) {'))
# Execute the actual renderer's marker decision, before any graphics calls.
a = s.index('static void desenharAcoesEpisodioCorpo(void){')
b = s.index('  // A CAIXA', a)
parts.append(s[a:b] + '\n  renderVisivel = trecho;\n}')
# A new source must clear the "already played" latch.
assert 'skipLiberado = 0;' in function('static void tocarFonteInterno(const char *url) {'), 'tocarFonteInterno keeps skipLiberado'
Path(sys.argv[1], 'decision.c').write_text('\n'.join(parts))
PY
for platform in host android; do
  flags=()
  if [ "$platform" = android ]; then flags+=(-DNV_ANDROID); fi
  cc "${flags[@]}" tests/player_skip_loading.c -I"$work" -o "$work/test"
  "$work/test"
done
