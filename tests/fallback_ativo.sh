#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "libass=0" do painel novo na faixa JA ABERTA (correcao F102, bloqueio 2 da
# revisao PR734CD-r2): a troca de motor repensa a preferencia sem reabrir a
# faixa — no antigo o documento volta ao libass, na volta ao novo ele sai de
# novo. Ver tests/fallback_ativo.c. Precisa do libass do Homebrew; sem ele,
# pula.
#
#   bash tests/fallback_ativo.sh
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "fallback_ativo: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_TPK -DNV_ASS_LIBASS -DNV_ASS_NOVO -include tests/ass_pisca_gl.h -Isrc -Itests \
  tests/fallback_ativo.c src/assperf.c src/asspeso.c src/assfios.c src/assrender.c src/legenda.c src/plainass.c \
  -o ${TMPDIR:-/tmp}/nuvio-fallback-ativo-test \
  $(pkg-config --cflags --libs libass) -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lGLESv2 -lfribidi -lpthread -lm \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fallback-ativo-test 2>/dev/null
