#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-tmdb.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
for alvo in nativo NV_ANDROID NV_TPK40; do
  flags=()
  [ "$alvo" = nativo ] || flags+=(-D"$alvo")
  [ "$alvo" != NV_ANDROID ] || flags+=(-Wno-unused-function)
  cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc ${flags[@]+"${flags[@]}"} tests/rede_tmdb.c src/redeurl.c -pthread -o "$tmp/teste"
  "$tmp/teste" > "$tmp/log"
  cat "$tmp/log"
  test "$(rg -c '^\[rede\] api.themoviedb.org bloqueado aqui; usando api.tmdb.org$' "$tmp/log")" = 1
done
