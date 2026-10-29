#!/bin/bash
# #205: PNGs of the USB cache modal (3 states). Needs a GL window.
#   bash tests/usbmodal_shot.sh /Volumes/ExternalSSD/tmp/usbmodal after
# USBMODAL_ROOT=<tree> USBMODAL_ANTES=1 builds an older tree (harness copied there).
set -eu
cd "$(dirname "$0")/.."
here=$PWD
root="${USBMODAL_ROOT:-$here}"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-usbmodal.XXXXXX")
trap 'rm -rf "$work"' EXIT
[ "$root" = "$here" ] || { mkdir -p "$root/tests"; cp tests/usbmodal_shot.c "$root/tests/"; }
sources=()
for source in "$root"/src/*.c "$root"/src/dts/*.c; do
  case "$source" in */src/main.c|*/src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc ${USBMODAL_ANTES:+-DUSBMODAL_ANTES} -DAJUSTES_TESTE "${sources[@]}" "$root/tests/usbmodal_shot.c" -I"$root/src" -o "$work/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "${1:-/tmp}"
NUVIO_DADOS="$work" "$work/shot" "${1:-/tmp}" "${2:-usbmodal}"
