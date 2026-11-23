#!/bin/bash
# Desktop Settings r13: hover/click on a row, Cmd+, routing, density gate.
# Full NV_DESKTOP_UI build, hidden GL window, no network.
#   bash tests/desktop_settings_r13.sh [--baseline <commit>]
# --baseline builds this test against src/ of that commit (proof it failed before).
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
cd "$(dirname "$0")/.."
work="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-settings-r13.XXXXXX")"
trap 'rm -rf "$work"' EXIT
root="$PWD"
if [ "${1:-}" = --baseline ]; then
  git archive "$2" src | tar -x -C "$work"
  root="$work"
  # Keep capture isolation and the AppKit pixel-test bridge while testing old layout.
  cp src/dados.c src/desktop_mac.m src/desktop_native.h "$root/src/"
fi
# Cmd+, has one owner: the guarded handler in app.c (none in main.c).
if grep -q SDLK_COMMA "$root/src/main.c"; then echo "FAIL: main.c intercepts Cmd+, before app_evento"; fail=1; fi
grep -q 'SDLK_COMMA' "$root/src/app.c"
flags=(-DNV_DESKTOP_RELEASE -DNV_DESKTOP_UI -DNV_TOUCH_UI -DAJUSTES_TESTE -I"$root/src" -Itests -O1 -g
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
sources=()
for source in "$root"/src/*.c "$root"/src/dts/*.c; do
  case "$source" in */main.c|*/ajustes.c|*/app.c) continue;; esac
  sources+=("$source")
done
cc -c "$root/src/app.c" "${flags[@]}" -Dplayer_aberto=nvt_player_aberto -o "$work/app.o"
sed "s#../src/ajustes.c#$root/src/ajustes.c#" tests/desktop_settings_r13.c > "$work/t.c"
# Compile shared sources once; an optional offline capture app reuses the objects.
objects=()
objectdir="$work"
if [ -n "${NV_DESKTOP_SETTINGS_BIN:-}" ]; then
  objectdir="$NV_DESKTOP_SETTINGS_BIN.objects"
  mkdir -p "$objectdir"
fi
for source in "${sources[@]}" "$root/src/desktop_mac.m"; do
  object="$objectdir/$(basename "${source%.*}").o"
  cc -c "$source" "${flags[@]}" -o "$object"
  objects+=("$object")
done
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -framework Cocoa -framework QuartzCore)
cc "${objects[@]}" "$work/app.o" "$work/t.c" "${flags[@]}" -o "$work/t" "${libs[@]}"
"$work/t"
if [ -n "${NV_DESKTOP_SETTINGS_BIN:-}" ]; then
  cc -c "$root/src/app.c" "${flags[@]}" -o "$work/app-real.o"
  cc -c "$root/src/ajustes.c" "${flags[@]}" -o "$work/ajustes.o"
  cc -c "$root/src/main.c" "${flags[@]}" -o "$work/main.o"
  frameworks="${NUVIO_DESKTOP_CACHE:-/Volumes/ExternalSSD/tmp/rev/desktop/cache}/frameworks"
  cc "${objects[@]}" "$work/app-real.o" "$work/ajustes.o" "$work/main.o" -o "$NV_DESKTOP_SETTINGS_BIN" \
    -F"$frameworks" -framework SDL2 -framework SDL2_image -framework SDL2_ttf -lz \
    -framework OpenGL -framework Cocoa -framework QuartzCore -Wl,-rpath,"$frameworks"
  mkdir -p "$NV_DESKTOP_SETTINGS_BIN.objects"
  cp "$work/app-real.o" "$work/ajustes.o" "$work/main.o" "$NV_DESKTOP_SETTINGS_BIN.objects/"
fi
exit "${fail:-0}"
