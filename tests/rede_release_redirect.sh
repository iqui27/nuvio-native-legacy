#!/bin/bash
# Redirects do download verificado de release: so https e so hosts oficiais.
# ?token= so no fragmento nao e credencial. Ver tests/rede_release_redirect.c.
#   bash tests/rede_release_redirect.sh [--sanitize]
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-redirect.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-O1 -g -Isrc -pthread -Wno-deprecated-declarations)
if [ "${1:-}" = --sanitize ] || [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
for platform in native NV_TPK NV_TPK40; do
  platform_flags=()
  [ "$platform" = native ] || platform_flags+=(-D"$platform")
  cc "${flags[@]}" "${platform_flags[@]}" tests/rede_release_redirect.c src/redeurl.c src/js.c -o "$DIR/t-$platform" -ldl -lm
  "$DIR/t-$platform"
done
