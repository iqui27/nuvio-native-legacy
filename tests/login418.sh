#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
dir=$(mktemp -d "$TMPDIR/login418.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -Wall -Wextra)
if [ "${1:-}" = --sanitize ] || [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc "${flags[@]}" tests/login418.c src/js.c src/jsw.c -o "$dir/test"
"$dir/test" > "$dir/log" 2>&1 || { cat "$dir/log"; exit 1; }
if grep -q SECRET "$dir/log"; then echo 'login418: secret leaked'; exit 1; fi
cat "$dir/log"
for platform in native NV_TPK NV_TPK40; do
  platform_flags=()
  [ "$platform" = native ] || platform_flags+=(-D"$platform")
  cc "${flags[@]}" "${platform_flags[@]}" tests/login418_rede.c src/redeurl.c -o "$dir/rede"
  "$dir/rede" > "$dir/rede-log" 2>&1 || { cat "$dir/rede-log"; exit 1; }
  if grep -q SECRET "$dir/rede-log"; then echo 'login418: transport secret leaked'; exit 1; fi
  cat "$dir/rede-log"
done
grep -q '\[login\].*class=timeout.*curl=28.*backoff_ms=1000' "$dir/log"
grep -q '\[login\].*class=DNS' "$dir/log"
grep -q '\[login\].*class=TLS' "$dir/log"
grep -q '\[login\].*class=HTTP.*http=503' "$dir/log"
