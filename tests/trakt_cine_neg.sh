#!/bin/bash
# Row 22 (mapa-erros-logs): "episodio ausente na ficha Nuvio; Cinemeta tambem
# nao tem" reimprimia o MESMO tt SxEy a cada sync (329 linhas em 14 registros,
# 155 num so) e, quando o Cinemeta nao tinha ficha, repetia o GET por serie.
# Cache negativo de 24 h: por EPISODIO para a linha, por SERIE para o GET.
#   bash tests/trakt_cine_neg.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt-cine-neg.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_cine_neg.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wextra -Wl,-dead_strip -lpthread
rc=0
for scenario in '' positive-expiry missing-expiry transient-retry html-retry malformed-retry truncated-retry error-retry no-meta-retry null-meta-retry error-meta-retry; do
  "$dir/teste" ${scenario:+"$scenario"} 2>&1 | grep -E 'trakt_cine_neg|FAIL' || rc=1
done
exit "$rc"
