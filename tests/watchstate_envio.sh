#!/bin/bash
# watch_state v2 push, passo 3 (fio de entrega com rede/disco/addons falsos).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ws-envio.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Executa o trecho real do arranque, sem janela/GL nem o restante da UI.
python3 - "$work/arranque.c" <<'PY'
from pathlib import Path
import sys
source = Path('src/main.c').read_text()
start = source.index('  addons_carregar(dirArte);')
end = source.index('  plugins_iniciar();', start)
Path(sys.argv[1]).write_text(
    '#include "addons.h"\n#include "watchstate_envio.h"\n'
    'void arranqueAddons(const char *dirArte) {\n' + source[start:end] + '}\n')
PY
cc tests/watchstate_envio.c "$work/arranque.c" src/watchstate_envio.c src/watchstate.c src/js.c src/jsw.c -Isrc -std=gnu11 -Wall -Wextra \
  -Wno-unused-function -Wno-macro-redefined -lpthread -o "$work/t"
"$work/t" "$@"
if [ "$#" -eq 0 ]; then
  "$work/t" --arranque
  "$work/t" --url-longa
  "$work/t" --query
  "$work/t" --sem-addon
  "$work/t" --arranque --sem-addon
  "$work/t" --outra-conta
  "$work/t" --outro-perfil
  "$work/t" --herdado
  "$work/t" --durante-envio
fi
