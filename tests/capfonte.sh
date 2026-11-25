#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Creditos por FONTE (F57): o marcador de um titulo nao pode vazar para o
# seguinte. Compila os trechos reais de video_tizen.c/video_html5.c/
# player.c/posplay.c extraidos por tests/capfonte.py — mesma tecnica do
# ensaio da revisao r2 — e mede:
#
#   1. sonda .wgt que terminou DEPOIS da troca de fonte nao publica na nova;
#   2. MP4 seguinte nao herda o capitulo do MKV (efetivo/painel em 0);
#   3. HTML5 com pthreads entrega o nomeado local ao credEfetivo;
#   4. no painel do filme o nomeado do arquivo ganha do provedor.
#
#   bash tests/capfonte.sh
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "$TMPDIR/nuvio-capfonte.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 tests/capfonte.py "$(pwd)" > "$dir/capfonte.c"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} "$dir/capfonte.c" src/intro.c src/segfontes.c src/js.c src/credfonte.c src/capmkv.c src/mkv.c \
  -DNV_SHOT_HOOKS -Isrc -o "$dir/capfonte" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$dir/capfonte"
