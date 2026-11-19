#!/bin/bash
# #725 characterization: simulated webOS5 DTS capability × hide × auto-pick (no GL).
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
SF_DIR=$(mktemp -d "${TMPDIR}/nuvio-fv725-webos-dts.XXXXXX")
trap 'rm -rf "$SF_DIR"' EXIT
flags=(-DNV_WEBOS -O1 -g -Isrc -pthread -I/opt/homebrew/include
       -Wall -Wextra -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/fv725_webos_dts.c \
  src/badges.c src/fontefiltro.c src/fonteauto.c src/fontenota.c src/vazao.c src/linguas.c \
  src/streamfit.c src/fontefalha.c src/limpa.c src/dobra.c src/addonmodo.c src/focus.c src/vaitocar.c \
  -lm -o "$SF_DIR/test"
"$SF_DIR/test"
