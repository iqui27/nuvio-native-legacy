#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Binge group na abertura MANUAL de outro episodio (issue #310). Dois niveis:
#   1. contrato de app.c: os tres caminhos manuais terminam em
#      `aguardandoFonte = 1`, e o bloco unico que nasce dai chama
#      fontepref_escolher + stream_preferir;
#   2. comportamento: tests/fontepref_manual.c (fontepref_escolher por id manual).
#
#   bash tests/fontepref_manual.sh
set -euo pipefail
cd "$(dirname "$0")/.."

falha=0
confere() { if [ "$1" -ge "$2" ]; then echo "ok    $3"; else echo "FALHA $3"; falha=1; fi; }

# Cada bloco manual: a janela de 6 linhas depois do gatilho tem de armar a espera.
for gatilho in 'episodios_escolheu(&t,&e)' 'player_pediu_proximo(&t,&e)' 'detail_pediu_reproduzir() || doInicio'; do
  n=$(grep -n -F "$gatilho" src/app.c | head -1 | cut -d: -f1)
  janela=$(sed -n "${n},$((n + 80))p" src/app.c | grep -c 'aguardandoFonte *= *1' || true)
  confere "$janela" 1 "caminho manual '$gatilho' arma aguardandoFonte = 1"
done
# O bloco central aplica a lembrada (binge incluso) ANTES de pedir a fonte.
n=$(grep -n 'lembrada = base\[0\] ? (ajustes_fonte_manual() ? fontepref_escolher_manual(base)' src/app.c | head -1 | cut -d: -f1)
confere "$(sed -n "${n},$((n + 3))p" src/app.c | grep -c 'fontepref_escolher(base)) : -1')" 1 "bloco central usa fontepref_escolher (com a automatica) fora do modo manual"
confere "$(sed -n "${n},$((n + 3))p" src/app.c | grep -c 'stream_preferir(lembrada)')" 1 "bloco central: fontepref_escolher -> stream_preferir"
# A lembrada so e gravada na escolha MANUAL da folha (decisao documentada).
confere "$(grep -c 'fontepref_guardar(base, s)' src/app.c)" 1 "fontepref_guardar so no escolheu da folha"
[ "$falha" = 0 ] || exit 1

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fontepref-manual.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/fontepref.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/fontepref_manual.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-fontepref-manual \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fontepref-manual "$@"
