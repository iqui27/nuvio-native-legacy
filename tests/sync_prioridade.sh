#!/bin/bash
# #497: progress, Home structure, profile ownership and fallback in actual sync.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sync-prioridade.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -D_GNU_SOURCE -O1 -g -Isrc -Itests -pthread
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -ffunction-sections -fdata-sections)
gc=(-Wl,--gc-sections)
if [ "$(uname -s)" = Darwin ]; then gc=(-Wl,-dead_strip); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  if [ "$(uname -s)" != Darwin ]; then flags+=(-fno-pie); gc+=(-no-pie); fi
fi
"${CC:-cc}" "${flags[@]}" -include tests/sync_prioridade_inject.h -c src/sync.c -o "$work/sync.o"
"${CC:-cc}" "${flags[@]}" "$work/sync.o" tests/stub_contapend.c src/catordem.c \
  src/catordemcache.c src/contacache.c src/js.c src/jsw.c tests/sync_prioridade.c \
  "${gc[@]}" -o "$work/test"
for scenario in concurrent stale thread-failure empty failed warm backoff; do
  mkdir "$work/data-$scenario"
  NV_T_DIR="$work/data-$scenario" \
    ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1}" \
    UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
    "$work/test" "$scenario"
done
echo 'sync_prioridade: seven scenarios passed'
