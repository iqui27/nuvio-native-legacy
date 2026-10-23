#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibfonte.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
root="$PWD"
flags=()
if [ -n "${BIBFONTE_REV:-}" ]; then
  mkdir -p "$tmp/tree/tests"
  git archive "$BIBFONTE_REV" src | tar -x -C "$tmp/tree"
  cp tests/bibfonte.c tests/bibfonte_library.c tests/bibfonte_discovery.c tests/bibfonte_menu.c tests/bibfonte_detail.c tests/bibfonte_shots.inc tests/nvtmp.h "$tmp/tree/tests/"
  root="$tmp/tree"
  if ! rg -q listaFontes "$root/src/catalogo.h"; then flags+=(-DBIBFONTE_BASELINE); fi
fi
# Compile the production Detail save action without running the app frame loop.
python3 - "$root/src/app.c" "$tmp/detail-action.c" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
action = source.split('    if (detail_pediu_marcar()) {', 1)[1].split('    if (detail_pediu_fontes()) {', 1)[0]
Path(sys.argv[2]).write_text('''#include "catalogo.h"
#include "ajustes.h"
#include "biblioteca.h"
#include "salvos.h"
#include "atividade.h"
#include "ilhasalvar.h"
#include "trakt.h"
#include "simkl.h"
#include <stdio.h>
int test_trakt_write(const char *, const char *, int);
int test_simkl_write(const char *, const char *, int);
#define detail_indice() index
#define trakt_watchlist(id, add) test_trakt_write(id, "series", add)
#define simkl_lista_tipo test_simkl_write
void bibfonte_app_toggle(int index) {
''' + action)
PY
sources=()
for source in "$root"/src/*.c "$root"/src/dts/*.c; do
  case "${source##*/}" in main.c|ajustes.c|ilhasalvar.c|biblioteca.c|descoberta.c|ctxmenu.c|detail.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]+${flags[@]}}" "${sources[@]}" "$root/tests/bibfonte.c" "$root/tests/bibfonte_library.c" "$root/tests/bibfonte_discovery.c" "$root/tests/bibfonte_menu.c" "$root/tests/bibfonte_detail.c" "$tmp/detail-action.c" \
  -I"$root/src" -O0 -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$tmp/test" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/test" "$@"
if [ "$#" -gt 0 ]; then "$tmp/test"; fi
