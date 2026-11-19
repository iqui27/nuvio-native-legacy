#!/bin/bash
# #551: replay same-profile generation invalidation while Home is open.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/recovery.inc" <<'PY'
from pathlib import Path
import sys
s = Path('src/app.c').read_text()
start = s.index('  if (tela == TELA_ESCOLHA_PERFIL && perfilsel_concluido() &&') if '  // Recover every logged-in screen' not in s else s.index('  if (sessao_logada() &&', s.index('  // Recover every logged-in screen'))
end = s.index('  // Os vinculos de Trakt', start)
Path(sys.argv[1]).write_text(s[start:end])
PY
cc -DNV_DESC_MIN_MS=1000ull -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/desc_sessao_recovery.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -Itests -I"$tmp" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$tmp/recovery" -O1 -g -pthread -Wl,-undefined,dynamic_lookup \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$tmp/recovery"
