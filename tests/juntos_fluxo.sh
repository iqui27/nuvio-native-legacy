#!/bin/bash
# Lobby entry contract and host + two guests through the production sync engine.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
ui = Path('src/juntosui.c').read_text()
entry = ui[ui.index('void juntosui_abrir_folha('):ui.index('// Linhas da folha')]
assert 'abrirSala();' in entry, 'Watch Together must open/preload the lobby immediately'
opening = ui[ui.index('static void abrirSala(void) {'):ui.index('int juntosui_pediu_reproduzir')]
assert 'CAMADA_SALA' in opening, 'preload must retain the lobby'
player = Path('src/player.c').read_text()
assert 'juntos_precarregando()' in player, 'player must prepare paused behind the lobby'
print('entry: Detail and episode actions share immediate lobby/preload; player holds pause')
PY
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-flow.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_fluxo.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/flow"
"$work/flow"
node tests/juntos_fluxo.mjs
node --test servidor/juntos/teste-logica.mjs
bash tests/juntos_sim.sh
node tests/tizen-avplay-contract.cjs
cc -fsyntax-only src/app.c -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
