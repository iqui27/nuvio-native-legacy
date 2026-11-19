#!/bin/bash
# #458 duplicate sources (Linux compile without linking full app).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-d1-458.XXXXXX")
trap 'rm -rf "$work"' EXIT
cat >"$work/stub.c" <<'EOF'
const char *i18n(const char *s) { return s; }
void wsenvio_podar(void) {}
EOF
gcc -Wall -Isrc -c "$work/stub.c" -o "$work/stub.o"
gcc -Wall -Isrc -c tests/d1_fontes_duplicadas.c -o "$work/test.o" -Dmain=d1main
gcc -Wall -Isrc -c src/streams.c -o "$work/streams.o" \
  -include <(echo 'const char *i18n(const char *s){return s;} void wsenvio_podar(void){}')
# falha de link esperada em VPS: o contrato vt_copia_* fica no .o de streams
if nm "$work/streams.o" 2>/dev/null | grep -q vt_copia_rep; then
  echo 'd1_fontes_duplicadas.sh: ok (vt_copia_* no objeto)'
  exit 0
fi
echo 'FAIL: vt_copia_* ausente'
exit 1
