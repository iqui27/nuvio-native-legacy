#!/bin/bash
# Android ordering: island/session update precedes the app's player handoff.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-tcl.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -DNV_ANDROID -DJUNTOS_TCL -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_fluxo.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/flow"
"$work/flow"
