#!/bin/bash
# Offline Home/Settings PNGs, sharpness regression, alpha equality and GPU timer.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ui720.XXXXXX")
trap 'rm -rf "$work"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/rev/ui720}"
mkdir -p "$out" "$work/data"
# Existing Android SDK only; never install a compiler/runtime for this check.
angle="$HOME/Library/Android/sdk/emulator/lib64/gles_angle"
ndk=$(ls -d "$HOME"/Library/Android/sdk/ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include 2>/dev/null | tail -1 || true)
if [ -f "$angle/libGLESv2.dylib" ] && [ -n "$ndk" ]; then
  mkdir -p "$work/inc"
  for d in GLES2 EGL KHR; do ln -s "$ndk/$d" "$work/inc/$d"; done
  cc tests/ui720_gles.c -Isrc -I"$work/inc" -L"$angle" -lEGL -lGLESv2 \
    -Wl,-rpath,"$angle" -Wl,-headerpad_max_install_names -o "$work/gles"
  for l in libEGL libGLESv2; do
    install_name_tool -change "./$l.dylib" "$angle/$l.dylib" "$work/gles"
  done
  codesign -f -s - "$work/gles" 2>/dev/null
  "$work/gles"
else
  echo "SKIP GLES2 compiler check: existing ANGLE/NDK unavailable"
fi
python3 tests/ui720_surface.py
printf 'animacoes 0\nidioma 1\nhomeLayoutLocal 0\n' > "$work/data/ajustes.txt"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/gfx.c|src/gpunivel.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
# Compile independent translation units in parallel; no optimization benchmark needed.
mkdir -p "$work/src/dts" "$work/tests"
printf '%s\0' "${sources[@]}" tests/ui720.c tests/ui720_settings.c |
  xargs -0 -P 8 -I '{}' cc '{}' -c -o "$work/{}.o" -DAJUSTES_TESTE -Isrc -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
cc "$work"/src/*.o "$work"/src/dts/*.o "$work"/tests/*.o -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
ui_rc=0
NUVIO_DADOS="$work/data" "$work/test" "$out" "${@:2}" > "$out/results.txt" 2>&1 || ui_rc=$?
rg '^(GPU |METRIC |BLIT |FRAME |CHOICE |PASS |FAIL |\[gpu-nivel\] ampliar=)' "$out/results.txt"
[ "$ui_rc" -eq 0 ]
if [ "$#" -lt 2 ]; then
  NUVIO_DADOS="$work/data" "$work/test" "$out" contracts > "$out/contracts.txt" 2>&1 || ui_rc=$?
  rg '^(PASS |FAIL )' "$out/contracts.txt"
  [ "$ui_rc" -eq 0 ]
fi
