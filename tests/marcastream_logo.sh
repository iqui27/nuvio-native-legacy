#!/bin/bash
# Recuperacao do wordmark do streaming (marcastream_logo.c): falha nao e
# definitiva — re-tenta com a chave do TMDB ativada ou apos o intervalo.
#   bash tests/marcastream_logo.sh
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-marcastream-logo.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
cc -O1 -g -Wall -Wextra -Isrc tests/marcastream_logo.c src/js.c -o "$bin" -lpthread
"$bin"
