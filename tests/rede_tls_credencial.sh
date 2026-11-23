#!/bin/bash
# Pedido com credencial verifica o TLS e nao segue redirect. Ver tests/rede_tls_credencial.c.
#   bash tests/rede_tls_credencial.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-tls.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-O1 -g -Isrc -pthread -Wno-deprecated-declarations)
if [ "${1:-}" = --sanitize ] || [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
for platform in native NV_TPK NV_TPK40; do
  platform_flags=()
  [ "$platform" = native ] || platform_flags+=(-D"$platform")
  cc "${flags[@]}" "${platform_flags[@]}" tests/rede_tls_credencial.c src/redeurl.c src/js.c -o "$DIR/t" -ldl -lm
  "$DIR/t"
done
