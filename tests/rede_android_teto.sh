#!/bin/bash
# Teto de bytes na reserva HTTP do Android (viaAndroid / android_http_teto).
#   bash tests/rede_android_teto.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-teto.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Wno-unused-function -Isrc "${san[@]}" \
  tests/rede_android_teto.c src/redeurl.c src/homekit.c src/js.c -pthread -o "$tmp/test"
"$tmp/test"
