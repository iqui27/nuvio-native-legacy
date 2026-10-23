#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-pedido.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=DNS:localhost -keyout "$tmp/key.pem" \
  -out "$tmp/cert.pem" > "$tmp/cert-generation.log" 2>&1
python3 tests/rede_pedido_server.py "$tmp/ports" "$tmp/cert.pem" "$tmp/key.pem" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/ports" ] && break; sleep 0.1; done
test -s "$tmp/ports"
base=$(sed -n '1p' "$tmp/ports")
tlsbase=$(sed -n '2p' "$tmp/ports")
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread)
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
if [ "${NV_TPK40_TEST:-0}" = 1 ]; then
  printf '%s\n' 'void nv_tpk40_etapa(const char *);' > "$tmp/tpk40.h"
  flags+=(-DNV_TPK40 -include "$tmp/tpk40.h")
  cc "${flags[@]}" -c src/rede.c -o "$tmp/rede40.o"
  if nm "$tmp/rede40.o" | rg 'tlv_bootstrap|tls_get_addr|emutls'; then
    echo 'TPK40 compiler TLS detected' >&2; exit 1
  fi
fi
cc "${flags[@]}" tests/rede_pedido.c src/rede.c src/redeurl.c -o "$tmp/teste"
"$tmp/teste" "$base" "$tlsbase" "$tmp/cert.pem"
cc "${flags[@]}" tests/rede_pedido_buffer.c src/redeurl.c -o "$tmp/buffer"
"$tmp/buffer"
node tests/rede_retry_wgt.cjs
node tests/rede_fluxo_wgt.cjs
