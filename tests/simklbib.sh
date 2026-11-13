#!/bin/bash
# Biblioteca do Simkl no disco (src/simklbib.c). Sem SDL, sem rede: disco em
# memoria. SANITIZE=1 liga ASan/UBSan.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}"; out="${out%/}/nuvio-simklbib-tests"
cc ${flags[@]+"${flags[@]}"} src/simklbib.c tests/simklbib.c -Isrc \
  -o "$out" -O1 -g -Wall -Wextra
"$out"
