#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fallback do motor antigo (PR 734 D, bloqueio 3 da revisao): libass=0 gravado
# pelo painel novo nao pode sumir com os letreiros no motor antigo. Ver
# tests/assmotor_fallback.c.
#
#   bash tests/assmotor_fallback.sh
set -eu
cd "$(dirname "$0")/.."
cc -DNV_TPK -D_GNU_SOURCE -Isrc -O1 -g -ffunction-sections -fdata-sections \
  tests/assmotor_fallback.c src/legenda.c src/assperf.c \
  -Wl,--gc-sections -lpthread -lm \
  -o ${TMPDIR:-/tmp}/nuvio-assmotor-fallback-test \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-assmotor-fallback-test
