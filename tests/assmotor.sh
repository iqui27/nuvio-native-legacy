#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Chave "Motor de legendas ASS" dos Ajustes (.tpk, PR 734 etapa D):
#   - parte leve (tests/assmotor_leve.c): padrao ligado sem dono, troca ao vivo
#     por ass_novo_registrar e os ASSPERF_* de runtime; layout de MkvFaixa
#     (quadroNs + transferencia no fim, 176/168 conforme o long).
#   - parte pesada (tests/assmotor.c): o ajuste de verdade — padrao ligado,
#     Desligado volta ao motor antigo, assMotorNovoLocal persiste no
#     ajustes.txt e e relido no arranque seguinte; ultima posicao do enum.
#   bash tests/assmotor.sh
set -eu
cd "$(dirname "$0")/.."
O="$TMPDIR/nuvio-assmotor"; mkdir -p "$O"

# Leve: so assperf.c basta (NV_ASS_NOVO liga o caminho novo sem NV_TPK, que
# puxaria dados.c).
cc -DNV_ASS_NOVO -DNV_ASSPERF_TESTE -Wall -Wextra -Isrc -O1 -g \
  src/assperf.c tests/assmotor_leve.c -o "$O/leve" -lpthread -lm
"$O/leve"

# Pesada: o ajustes.c de verdade sobre toda a arvore (padrao tests/ajustes_*).
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp ${TMPDIR:-/tmp}/nuvio-assmotor-pesado.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc -DAJUSTES_TESTE -DNV_ASS_NOVO "${sources[@]}" tests/assmotor.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
