#!/bin/bash
# #364: preferencia cai para a proxima fonte; escolha manual nao; sonda sem URL completa.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fonteescolha.XXXXXX")
trap 'rm -rf "$dir"' EXIT
sani=""; [ "${SANITIZE:-0}" = 1 ] && sani="-fsanitize=address,undefined"
cc -O1 -g -Wall -Isrc $sani \
  src/fontesonda.c tests/fonteescolha.c -o "$dir/t"
"$dir/t"
# A fiacao em app.c: a escolha e a preferencia viram automatico, a manual
# continua com o prazo manual, e a sonda roda nos dois caminhos de erro.
grep -q 'fonteescolha_cai_para_proxima(fonte, escolhaPreferida' src/app.c
grep -q 'else if (!player_id_canal()\[0\]) fonteManualDesde = SDL_GetTicks();' src/app.c
test "$(grep -c 'sondarAposRecusa();' src/app.c)" -ge 2
echo "ok fiacao app.c"
