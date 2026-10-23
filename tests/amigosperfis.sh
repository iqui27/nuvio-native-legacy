#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-profile-friends.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -Wall -Wextra -Werror -O1 -g -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
read -r -a libs <<< "$(pkg-config --libs sdl2)"
for suite in amigosperfis amigosperfis_compat; do
  cc "${flags[@]}" "${sdk[@]}" "tests/$suite.c" src/js.c -o "$work/test" "${libs[@]}"
  "$work/test"
done
