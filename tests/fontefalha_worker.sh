#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontefalha-worker.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -Isrc -pthread -Wall -Wextra -Wno-unused-function
       -ffunction-sections -fdata-sections -DNV_LINUX_DESKTOP)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  linker=(-Wl,-dead_strip)
else
  linker=(-Wl,--gc-sections)
  if pkg-config --exists sdl2; then
    read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
    flags+=("${sdk[@]}")
  fi
fi
# Explicit caller flags also support CPU-only SDK type shims; no SDL runtime
# functions are exercised by this worker regression.
if [ -n "${CFLAGS:-}" ]; then read -r -a extra <<< "$CFLAGS"; flags+=("${extra[@]}"); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/fontefalha_worker.c src/fontefalha.c src/fonteauto.c src/limpa.c src/dobra.c src/fontenota.c src/linguas.c src/fontefiltro.c \
  "${linker[@]}" -lm -o "$dir/teste"
"$dir/teste" "$dir"
