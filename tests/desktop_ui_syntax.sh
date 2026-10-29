#!/bin/bash
# Compile only the desktop UI owners; no app link, package, or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
for mode in desktop tv; do
  flags=(-std=gnu11)
  if [ "$mode" = desktop ]; then flags+=(-DNV_DESKTOP_UI -DNV_DESKTOP_RELEASE -DNV_TOUCH_UI); fi
  cc -std=gnu11 -fsyntax-only "${flags[@]}" src/main.c src/layout.c src/menu.c src/home.c src/app.c src/ajustes.c src/ponteiro.c src/busca.c src/biblioteca.c src/filmo.c src/plrui.c \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
done
if command -v x86_64-w64-mingw32-gcc >/dev/null; then
  CACHE="${NUVIO_DESKTOP_CACHE:-/Volumes/ExternalSSD/tmp/rev/desktop/cache}"
  includes=()
  for spec in SDL2-2.32.10 SDL2_image-2.8.8 SDL2_ttf-2.24.0; do
    includes+=(-I "$CACHE/$spec/x86_64-w64-mingw32/include" -I "$CACHE/$spec/x86_64-w64-mingw32/include/SDL2")
  done
  x86_64-w64-mingw32-gcc -std=gnu11 -fsyntax-only -DNV_DESKTOP_UI -DNV_DESKTOP_RELEASE -DNV_TOUCH_UI \
    -DSDL_MAIN_HANDLED -include tools/windows-compat/platform.h -I tools/windows-compat "${includes[@]}" \
    src/main.c src/layout.c src/menu.c src/home.c src/app.c src/ajustes.c src/ponteiro.c src/busca.c src/biblioteca.c src/filmo.c src/plrui.c \
    -Wno-incompatible-pointer-types -Wno-int-conversion
fi
