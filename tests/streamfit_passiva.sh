#!/bin/bash
# StreamFit passive ingestion gate + runtime parser (F03). SANITIZE=1 ASan/UBSan,
# SANITIZE=thread TSan. No network, JVM or device.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-streamfit-passiva.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -Werror -Isrc -pthread)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
cc "${flags[@]}" tests/streamfit_passiva.c src/streamfit.c src/streamfitpassiva.c src/streamfitdur.c \
  src/redemarca.c src/vazao.c -o "$dir/test"
"$dir/test" > "$dir/out"
grep -c "passive accepted" "$dir/out" >/dev/null
