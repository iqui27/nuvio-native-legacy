#!/bin/bash
# #344: a busca do guia nao pode ler alem do buffer quando um canal tem mais
# programas na janela do que cabe em ps[]. Ver tests/guiabusca_faixa.c.
#
#   bash tests/guiabusca_faixa.sh              # sem sanitizer
#   SANITIZE=1 bash tests/guiabusca_faixa.sh   # ASan + UBSan (o que prova o #344)
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiabusca-faixa-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/epg.c tests/guiabusca_faixa.c \
  -Isrc -o "$tmp/t" -O1 -g -lz -lpthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/t"
