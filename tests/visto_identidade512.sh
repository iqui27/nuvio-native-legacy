#!/bin/bash
# Real watched enqueue/worker, deterministic service boundaries, no network.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-visto-identidade512.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Isrc -pthread)
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a includes <<< "$(pkg-config --cflags sdl2)"
else
  includes=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Darwin ]; then
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1}"
  else
    flags+=(-no-pie)
  fi
fi
"${CC:-cc}" "${flags[@]}" "${includes[@]}" tests/visto_identidade512.c -o "$work/test"
"$work/test"
