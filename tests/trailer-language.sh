#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trailer-language.XXXXXX")
trap 'rm -rf "$build"' EXIT
flags=(-O1 -g -Wall -Wextra -Wno-unused-function -Isrc)
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
else
  sdl_flags=(-I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2)
fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for target in native android tpk wgt; do
  defs=()
  case "$target" in android) defs=(-DNV_ANDROID);; tpk) defs=(-DNV_TPK);; wgt) defs=(-D__EMSCRIPTEN__);; esac
  cc "${flags[@]}" "${defs[@]}" tests/trailer-language.c src/js.c "${sdl_flags[@]}" -lpthread -o "$build/$target"
  "$build/$target"
done
