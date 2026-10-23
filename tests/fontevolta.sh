#!/bin/bash
# Regras da fonte guardada para o Retomar. Sem rede: sonda falsa.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontevolta.XXXXXX")
trap 'rm -rf "$dir"' EXIT
# stream_url_serve vem de streams.c, que puxa o resto do app: aqui so a
# assinatura, porque o teste troca a sonda.
cat > "$dir/sonda.c" <<'C'
int stream_url_serve(const char *u, const char *c) { (void)u; (void)c; return 0; }
C
sani=""; [ "${SANITIZE:-0}" = 1 ] && sani="-fsanitize=address,undefined"
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  $sani \
  src/fontevolta.c "$dir/sonda.c" tests/fontevolta.c -o "$dir/t" -lpthread
"$dir/t"
