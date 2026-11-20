#!/bin/bash
# Synthetic host check/benchmark of the actual native production loader.
set -eu
cd "$(dirname "$0")/.."
base="${TMPDIR:-/tmp}"
dir=$(mktemp -d "${base%/}/webp-output.XXXXXX")
trap 'rc=$?; rm -rf "$dir"; exit "$rc"' EXIT
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# shellcheck disable=SC2046
cc -O2 -g -Wall -Wextra -pthread ${san[@]+"${san[@]}"} -Isrc \
  $(pkg-config --cflags --libs libwebp sdl2) tests/webp-output.c -o "$dir/test"
if [ -n "${KEEP_BINARY:-}" ]; then cp "$dir/test" "$KEEP_BINARY"; fi
"$dir/test"
