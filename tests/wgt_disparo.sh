#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-wgt-disparo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 tests/wgt_disparo.py "$(pwd)" > "$dir/test.c"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc "${flags[@]}" "$dir/test.c" src/intro.c src/segfontes.c src/js.c src/credfonte.c src/capmkv.c src/mkv.c \
  -DNV_SHOT_HOOKS -Isrc -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread -o "$dir/test"
"$dir/test"
