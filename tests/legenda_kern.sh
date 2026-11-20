#!/bin/bash
# Reproduce the TV renderer: SDL_ttf 2.22 without HarfBuzz, same FreeType.
# Optional PNG output path. Requires Homebrew SDL2, SDL2_image, FreeType, HarfBuzz.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/legenda-kern.XXXXXX")
trap 'rm -rf "$work"' EXIT
source_dir="${HOME}/.cache/nuvio-tpk/src/SDL2_ttf-2.22.0"
if [ ! -f "$source_dir/SDL_ttf.c" ]; then
  curl -fsSL https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz | tar -xz -C "$work"
  source_dir="$work/SDL2_ttf-2.22.0"
fi
cc tests/legenda_kern.c -o "$work/test" -O1 -g \
  -I"$source_dir" -I/opt/homebrew/include/SDL2 -I/opt/homebrew/include/freetype2 \
  -I/opt/homebrew/include/harfbuzz -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lfreetype -lharfbuzz -lm
"$work/test" "$@"
