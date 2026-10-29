#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-foracache.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
out = Path(sys.argv[1])
def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
p = Path('src/player.c').read_text()
s = Path('src/streams.c').read_text()
a = Path('src/app.c').read_text()
parts = [function(p, 'static void durFonteAtualizar(')]
parts += [function(s, 'static int mesmoAddon(')]
for signature in ('static int debridDaFonte(', 'int stream_automatico_excluir_aviso('):
    if signature in s:
        parts.append(function(s, signature))
    elif signature.startswith('int '):
        parts.append('int stream_automatico_excluir_aviso(int i) { (void)i; return 0; }')
# Execute the production auto-play entry, ending before ordinary transport failures.
start = a.index('static void tentarProximaFonteVOD(void) {')
end = a.index('  desde = SDL_GetTicks() - fonteVODDesde;', start)
parts.append(a[start:end] + '\n}')
out.joinpath('decision.c').write_text('\n'.join(parts))
PY
cc -std=c11 -Wall -Wextra -Wno-unused-variable -Wno-unused-function \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -I"$work" tests/player_foracache.c -lm -o "$work/test"
"$work/test"
