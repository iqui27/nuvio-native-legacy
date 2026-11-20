#!/bin/bash
# sync_resumo() le o resumo do ciclo sob a trava em que o fio o escreve. Ver o .c.
# Precisa de TSan (clang -fsanitize=thread): sem ele nao ha o que medir.
#
#   bash tests/sync_resumo_corrida.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
bin="$tmp/nuvio-sync-resumo-corrida"
cc -O1 -g -Isrc -Itests -pthread -fsanitize=thread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function \
  src/sync.c tests/stub_contapend.c src/catordem.c src/catordemcache.c src/contacache.c src/js.c src/jsw.c \
  tests/sync_resumo_corrida.c -o "$bin"
dir="$(mktemp -d "$tmp/nuvio-sync-resumo.XXXXXXXX")"
log="$dir/tsan.log"
trap 'rm -rf "$dir"' EXIT
# Outras leituras sem barreira de sync.c sao documentadas (estado, fioVivo):
# este teste so falha pelas corridas que passam por sync_resumo.
NV_T_DIR="$dir" TSAN_OPTIONS="exitcode=0 log_path=$log history_size=4" "$bin" | tail -1
if cat "$log".* 2>/dev/null | grep -qE '#[0-9]+ sync_resumo '; then
  cat "$log".* | grep -B3 -A8 -E '#[0-9]+ sync_resumo ' | head -40
  echo "sync_resumo_corrida: FALHOU (corrida no resumo)"; exit 1
fi
echo "sync_resumo_corrida: ok"
