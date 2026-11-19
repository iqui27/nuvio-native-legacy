#!/bin/bash
# Auth regression: long Nuvio JWT, missing auth recovery, Trakt refresh.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-auth.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -DNV_ANDROID -DJUNTOS_TCL -DJUNTOS_AUTH -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_fluxo.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/flow"
"$work/flow"
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_auth_headers.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/headers"
"$work/headers"
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  src/traktauth.c src/credfio.c src/js.c src/jsw.c tests/traktauth_renov.c \
  tests/juntos_auth_session.c -lpthread -o "$work/refresh"
"$work/refresh"
cc -O1 -g -DNV_ANDROID -DNV_REC_URL='"http://fixture.invalid"' -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_auth_refresh.c \
  src/recomenda.c src/trakt.c src/traktauth.c src/credfio.c \
  src/juntosfonte.c src/juntosmotor.c src/juntosproto.c src/juntossync.c \
  src/js.c src/jsw.c -L/opt/homebrew/lib -lSDL2 -lpthread -Wl,-dead_strip \
  -o "$work/room-refresh"
"$work/room-refresh"
