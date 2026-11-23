#!/bin/bash
# NOMEFONTE directed (Mac only — streams_desktop.inc needs NV_DESKTOP_UI &&
# __APPLE__): hidden GL window, offline, UBSan + Guard Malloc. The desktop
# sheet must rasterize the whole filename on the focused row.
#   bash tests/desktop_fontes_nome.sh     current tree
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/PlayportWork/nuvio-21/tmp}"
mkdir -p "$TMPDIR"
work=$(mktemp -d "$TMPDIR/nv-nomefonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/data" "$work/obj"
san=(-fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
flags=(-DNV_DESKTOP_RELEASE -DNV_DESKTOP_UI -DNV_TOUCH_UI -DNV_SHOT_HOOKS -Isrc -O1 -g "${san[@]}"
  -DNV_VERSAO='"2.1.0"' \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
export NV_OBJ="$work/obj"
sources=(src/desktop_mac.m)
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" tests/desktop_fontes_nome.c |
  xargs -P 4 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_OBJ/$(basename "$0").o"' '{}' "${flags[@]}"
cc "${san[@]}" "$work/obj/"*.o -o "$work/test" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework Cocoa -framework QuartzCore -framework OpenGL
# Bounded run (fio1-pilha.sh convention; macOS has no coreutils timeout).
perl -e 'alarm shift; exec @ARGV' 300 env DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib \
  MALLOC_PROTECT_BEFORE=1 MALLOC_STRICT_SIZE=1 \
  NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test"
