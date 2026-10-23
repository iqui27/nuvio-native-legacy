#!/bin/bash
# Sidebar Apple TV: rotulo mais pesado, cabecalho "Streaming" marcante, miolo
# do painel com piso de opacidade e relogio unico com a barra aberta.
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sidebaratv-fonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Extrai de text.c o enum PESO_*, a tabela ESTILOS e txt_estilo_medidas: os
# pesos/corpos que valem, sem linkar o SDL_ttf.
python3 - "$work/sidebaratv_fonte_impl.h" <<'PY'
from pathlib import Path
import sys
s = Path('src/text.c').read_text()
a = s.index('enum { PESO_REGULAR')
b = s.index('}', s.index('void txt_estilo_medidas(')) + 1
Path(sys.argv[1]).write_text(s[a:b] + '\n')
PY
cc tests/sidebaratv_fonte.c -Isrc -I"$work" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -Wl,-undefined,dynamic_lookup -o "$work/teste"
"$work/teste"
