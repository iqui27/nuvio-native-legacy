#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OK na Live TV abre o mini guia (TCL sem tecla colorida). Com um argumento,
# grava <arg>-faixa.png e <arg>-guia.png sobre o quadro de fixture.
#
#   bash tests/guiaoverlay_ok.sh [/tmp/x/shot]
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-guiaoverlay-ok.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

python3 - <<'PY'
from pathlib import Path
source = Path("android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt").read_text()
assert "KeyEvent.KEYCODE_GUIDE, KeyEvent.KEYCODE_MENU -> KeyEvent.KEYCODE_F10" in source
assert "KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> KeyEvent.KEYCODE_ENTER" in source
PY

# Exercise the actual app request/handoff bodies without login or network setup.
python3 - "$NUVIO_DADOS/guide_requests.h" <<'PY'
from pathlib import Path
import sys
source = Path('src/app.c').read_text()
start = source.index('static void guiaComCanalNoAr(void) {')
end = source.index('\n}\n', start) + 3
handoff = source[start:end]
start = source.index('  if (player_pediu_guia() && player_aberto()) {')
end = source.index('\n  }', start) + 4
band = source[start:end]
start = source.index('  { CatItem it;\n    if (aguardandoFonte != 2 && guia_pediu_preview(&it)')
end = source.index('\n  if (guia_pediu_parar_preview()', start)
new_preview = source[start:end]
start = source.index('  if (player_quer_sair() && player_eh_canal() && player_com_video() &&')
end = source.index('guiaComCanalNoAr();', start) + len('guiaComCanalNoAr();')
back_to_guide = source[start:end]
Path(sys.argv[1]).write_text('''
#define tela testGuideScreen
#define TELA_GUIA 1
#define trocarTela(screen) guia_abrir()
static int testGuideScreen = TELA_GUIA;
''' + handoff + '''
static void testGuideRequests(void) {
''' + band + '''
  if (player_pediu_guia_cheio() && player_aberto() && player_id_canal()[0]) guiaComCanalNoAr();
}
static void testBackToGuide(void) {
''' + back_to_guide + '''
}
#undef trocarTela
#undef TELA_GUIA
#undef tela
static int testPreviewConnections;
#define tocarCanal(item) (++testPreviewConnections)
static void testPreviewRequests(void) {
  int aguardandoFonte = 0;
''' + new_preview + '''
}
#undef tocarCanal
''')
PY

# Use a checked-in still frame for repeatable captures.
export NUVIO_SHOT_ARTE="${NUVIO_SHOT_ARTE:-design/glass-ilha/arte.jpg}"
if [ "$#" -gt 0 ]; then mkdir -p "$(dirname "$1")"; fi

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c) continue;; esac
  sources+=("$source")
done
# src/guia.c e INCLUIDO pelo teste: desenharCard e a lista de canais sao
# estaticos, e semear por dentro e o unico jeito de fotografar sem addon no ar.
cc "${sources[@]}" tests/guiaoverlay_ok.c -Isrc -I"$NUVIO_DADOS" -o ${TMPDIR:-/tmp}/nuvio-guiaoverlay-ok \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
if [ "$#" -eq 0 ]; then set -- "$NUVIO_DADOS/shot"; fi
${TMPDIR:-/tmp}/nuvio-guiaoverlay-ok "$@"
