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
# e6544a115 (owner): every pick in the sheet is exact, the remembered one too;
# the direct path arms the manual deadline and never falls back behind the screen.
grep -q 'if (manual) abrirFonteManualDireta(s);' src/app.c
grep -A10 '^static void abrirFonteManualDireta' src/app.c | grep -q 'fonteManualDesde = SDL_GetTicks();'
if grep -q 'fonteescolha_cai_para_proxima' src/app.c; then
  echo "FAIL fiacao app.c: manual pick falls back to the next source" >&2; exit 1
fi
test "$(grep -c 'sondarAposRecusa();' src/app.c)" -ge 2
echo "ok fiacao app.c"
