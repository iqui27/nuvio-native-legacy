#!/bin/bash
# #524: opposite-type shared-base-id progress application at cat_definir_tudo boundary.
# Synthetic catalogo+fixture runner. Author lease: no local compile/run; central verifier may execute.
#
#   SANITIZE=1 bash tests/fv524_progress_type.sh
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR}/nuvio-fv524-progress-type.XXXXXX")
trap 'rm -f "$out"' EXIT
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/fv524_progress_type.c \
  -Isrc -o "$out" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
