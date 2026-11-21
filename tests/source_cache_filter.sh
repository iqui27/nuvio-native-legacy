#!/bin/bash
# Persistent cache-state filter; local fixtures only.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/mnt/block-volume}/nuvio-source-cache.XXXXXX")
trap 'rm -rf "$work"' EXIT
compiler=${CC:-cc}
flags=(-O1 -g -Wall -Wextra -Werror)
if [[ -n "${CFLAGS:-}" ]]; then read -r -a extra <<< "$CFLAGS"; flags+=("${extra[@]}"); fi
sanitize=${NV_TEST_SANITIZE:-${SANITIZE:-0}}
if [[ "$sanitize" == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

# Runs on Linux and macOS without SDL. It exercises the exact classifier used
# by streams.c and then the production filter predicate.
"$compiler" "${flags[@]}" src/fontefiltro.c src/linguas.c \
  tests/source_cache_filter.c -Isrc -o "$work/classifier"
"$work/classifier"

# The manual Sources-sheet integration needs the complete SDL application.
# Keep this normal-only: compiling the complete application with sanitizers is
# prohibitively slow, while the portable test above sanitizer-checks the same
# production cache decisions.
if [[ "$sanitize" != 1 ]]; then
  sources=()
  for source in src/*.c src/dts/*.c; do
    [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
  done
  appflags=(-O1 -g)
  libs=()
  if [[ "$(uname -s)" == Darwin ]]; then
    appflags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
    libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
  else
    pkg-config --exists sdl2 SDL2_image SDL2_ttf
    appflags+=(-DNV_LINUX_DESKTOP)
    read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
    read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
    appflags+=("${sdk_flags[@]}")
    libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
  fi
  if [[ -n "${CFLAGS:-}" ]]; then appflags+=("${extra[@]}"); fi
  "$compiler" "${appflags[@]}" "${sources[@]}" tests/source_cache_filter_sheet.c \
    -Isrc -o "$work/sheet" "${libs[@]}" \
    -Wno-deprecated-declarations -Wno-macro-redefined
  mkdir "$work/data"
  NUVIO_DADOS="$work/data" "$work/sheet" "$work/data"
fi
