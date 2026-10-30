#!/bin/bash
# Headless menu mapping and precise-scroll regression checks.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-native-menu.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -ffunction-sections -fdata-sections -Wl,-dead_strip)
cc tests/desktop_native_menu.c "${flags[@]}" -Wl,-undefined,dynamic_lookup -o "$work/menu"
"$work/menu"
cc tests/desktop_input.c "${flags[@]}" -L/opt/homebrew/lib -lSDL2 -o "$work/input"
"$work/input"
cc -fsyntax-only -DNV_DESKTOP_UI -DNV_TOUCH_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/desktop_mac.m src/ctxmenu.c src/main.c src/detmais.c src/biblioteca.c src/salvospainel.c -Wno-deprecated-declarations
