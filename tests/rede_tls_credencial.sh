#!/bin/bash
# Pedido com credencial verifica o TLS e nao segue redirect. Ver tests/rede_tls_credencial.c.
#   bash tests/rede_tls_credencial.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-tls.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -O1 -g -Isrc -pthread -Wno-deprecated-declarations tests/rede_tls_credencial.c src/redeurl.c src/js.c -o "$DIR/t" -ldl -lm
"$DIR/t"
