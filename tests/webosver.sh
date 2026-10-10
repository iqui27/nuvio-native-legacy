#!/bin/sh
set -e
cd "$(dirname "$0")/.."
d=$(mktemp -d "${TMPDIR:-/tmp}/webosver.XXXXXX")
trap 'rm -rf "$d"' EXIT
cc -std=c11 -D_DEFAULT_SOURCE -DAJUSTES_TESTE -Wall -Wextra -Werror -Isrc tests/webosver.c src/webosver.c -pthread -o "$d/t"
"$d/t" "$d"
# Ultrareview do #408: o gancho de teste nao pode ir no binario de producao
# (ele troca os caminhos da versao do webOS que o DTS e o Dolby Vision leem).
cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Isrc -c src/webosver.c -o "$d/prod.o"
if nm "$d/prod.o" | grep -q "nv_webos_testar"; then
  echo "FAIL nv_webos_testar compilado sem AJUSTES_TESTE"; exit 1
fi
echo "ok   nv_webos_testar so existe com AJUSTES_TESTE"
