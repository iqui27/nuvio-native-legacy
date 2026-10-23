#!/bin/bash
# Offline #551 regression. Optional revision replays pre-fix app routing only.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pinrows.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
if [ "$#" -gt 0 ]; then
  git show "$1:src/app.c" > "$work/app.c"
else
  cp src/app.c "$work/app.c"
fi
python3 - "$work" <<'PY'
from pathlib import Path
import sys
work = Path(sys.argv[1])
app = (work / 'app.c').read_text()
picker = Path('src/perfilsel.c').read_text()
start = app.index('  // Recover every logged-in screen') if '  // Recover every logged-in screen' in app else app.index('  // A confirmed choice changes identity')
end = app.index('  // Os vinculos de Trakt', start)
(work / 'recovery.inc').write_text(app[start:end])
end = app.index('  // O fundo das listras', end)
start = app.rfind('    if (perfilsel_concluido()', 0, end)
# Include the completion branch only; frame() owns the surrounding returns.
end = app.index('\n    }', start) + len('\n    }')
(work / 'completion.inc').write_text(app[start:end])
fields = picker[picker.index('static char pin['):picker.index('static unsigned muralSorteio')]
start = picker.index('static void escolher(')
end = picker.index('static void eventoPin(', start)
(work / 'picker.inc').write_text(fields + picker[picker.index('static void *fioVerificar('):start] + picker[start:end])
start = picker.index('  { int resultado = atomic_load(&resultadoPin);')
end = picker.index('  if (foco >= perfis_n())', start)
(work / 'pin-result.inc').write_text(picker[start:end])
PY
flags=(-O1 -g -Isrc -Itests -I"$work" -pthread
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -ffunction-sections -fdata-sections -DNV_DESC_MIN_MS=0ull
  -Wno-deprecated-declarations -Wno-unused-function)
gc=(-Wl,--gc-sections)
if [ "$(uname -s)" = Darwin ]; then gc=(-Wl,-dead_strip); fi
cc "${flags[@]}" tests/pinrows.c src/perfis.c src/jsw.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/cwordem.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c \
  src/redeurl.c src/catordem.c "${gc[@]}" -o "$work/test"
result=0
for scenario in plain switch boot; do
  mkdir "$work/$scenario"
  NV_T_DIR="$work/$scenario" "$work/test" "$scenario" || result=1
done
exit "$result"
