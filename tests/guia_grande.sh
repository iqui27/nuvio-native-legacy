#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-grande.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wall -Wno-unused-function)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip)
else
  read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdl_flags[@]}" -Wl,--gc-sections)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for platform in tv android; do
  extra=();if [ "$platform" = android ]; then extra+=(-DNV_ANDROID); fi
  backup_extra=()
  if [ -f src/backupcategories.c ]; then backup_extra+=(src/backupcategories.c); fi
  cc ${backup_extra[@]+"${backup_extra[@]}"} "${flags[@]}" ${extra[@]+"${extra[@]}"} tests/guia_grande.c src/ltv.c src/guiaaddons.c src/guiafiltro.c src/js.c -lpthread -lm -o "$work/test"
  "$work/test" "$work"
done
