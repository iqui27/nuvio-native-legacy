#!/bin/bash
# Offline regression against real Settings and platform discovery; no window/network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ui720b.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
mkdir -p "$work/src/dts" "$work/tests"
printf '%s\0' "${sources[@]}" tests/ui720b.c |
  xargs -0 -P 8 -I '{}' cc '{}' -c -o "$work/{}.o" -Isrc -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
cc "$work"/src/*.o "$work"/src/dts/*.o "$work"/tests/*.o -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
python3 tests/ui720_surface.py
python3 tests/ui720b_host.py
for panel in 3840x2160 1920x1080 1280x720 7680x4320 invalid 0x2160 desktop4k; do
  env -u NUVIO_PANEL_SIZE -u NUVIO_PANEL_SOURCE "$work/test" "$panel"
done
cc tests/ui720b_panel.c src/js.c -DNV_WEBOS -Isrc -o "$work/panel" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2
"$work/panel"
# Compile both native discovery branches; no SDK installation or target access.
for platform in NV_WEBOS NV_TPK; do
  cc tests/ui720b.c -c -o "$work/$platform.o" -D"$platform" -Isrc -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
done
cc src/main.c -DNV_WEBOS -fsyntax-only -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
