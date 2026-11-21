#!/bin/bash
# PR 680 stage B: the .tpk 6+ / webOS seek-thumbnail provider (src/miniatura.c).
# Directed: the memory gate (the guide rule), the adaptive 5/10 s grid, the
# per-frame memory floor + measured cost, the bounded JPEG/texture caches, the
# stale-neighbour display and the lifecycle. Fails on the base (no miniatura.c).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-miniatura-tv.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Isrc -I/usr/include/SDL2)
if [ "${TSAN:-0}" = 1 ]; then
  # The fixture shares the worker's globals across threads — GCC TSan is
  # broken here, always Clang.
  flags+=(-fsanitize=thread)
  cc() { clang "$@"; }
elif [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

# The full provider under the .tpk 6+ flag set (TPK + the DTS FFmpeg).
cc "${flags[@]}" -DNV_TPK -DNV_DTS_FFMPEG tests/miniatura_tv.c -lpthread -lm -o "$work/miniatura_tv"
"$work/miniatura_tv"

# The guide rule on the .wgt: Auto is always "pouca" — the option never shows.
cc "${flags[@]}" -D__EMSCRIPTEN__ tests/miniatura_wasm.c -o "$work/wasm_gate"
"$work/wasm_gate"

# Packages WITHOUT the DTS FFmpeg keep only inert exports (TPK40, stale SDK):
cc "${flags[@]}" -DNV_TPK -fsyntax-only src/miniatura.c
cc "${flags[@]}" -DNV_WEBOS -fsyntax-only src/miniatura.c
# And the .wgt / desktop: the file is an empty translation unit.
cc "${flags[@]}" -fsyntax-only src/miniatura.c
