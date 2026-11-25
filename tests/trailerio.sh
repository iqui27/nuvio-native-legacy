#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -DNV_WEBOS -Isrc -pthread
       -ffunction-sections -fdata-sections)
if [[ ${SANITIZE:-0} == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
if [[ ${1:-} == --native ]]; then
  "${CC:-cc}" "${flags[@]}" tests/trailerio_native.c src/trailer.c src/trailerfonte.c \
    -Wl,--gc-sections -o "$scratch/native"
  "$scratch/native"
  exit
fi
"${CC:-cc}" "${flags[@]}" tests/trailerio.c src/trailerio.c src/js.c \
  -Wl,--gc-sections -o "$scratch/test"
"$scratch/test" "$@"
