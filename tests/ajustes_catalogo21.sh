#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Every option of the integracao/2.1 catalog (its TELA plus the Pencil pages)
# still has a route in Settings: a row, a composite editor that opens it, or a
# documented removal that nothing outside Settings reads.
#   bash tests/ajustes_catalogo21.sh [ref]
set -eu
cd "$(dirname "$0")/.."
ref="${1:-integracao/2.1}"
if ! git rev-parse --verify -q "$ref^{commit}" >/dev/null; then
  echo "SKIP: $ref not in this clone"; exit 0
fi
work="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-catalogo21.XXXXXX)"
trap 'rm -rf "$work"' EXIT
{
  git show "$ref:src/ajustes_ux_tela.inc" | grep -oE 'OPCA?\(AJ_[A-Z0-9_]*' | sed -E 's/OPCA?\(//'
  for f in $(git ls-tree --name-only "$ref" src/ | grep '^src/ajustes_pen'); do
    git show "$ref:$f" | grep -o '{AJ_[A-Z0-9_]*,"' | sed 's/[{,"]//g'
  done
} | sort -u > "$work/ops"
[ "$(wc -l < "$work/ops")" -gt 300 ] || { echo "FAIL: catalog of $ref looks truncated"; exit 1; }
# Keys declared only in the Android enum are checked by the Android build.
guard() { sed '/AJ_BAIXAR_REGISTRO/{s/^/#ifdef NV_ANDROID\n/;s/$/\n#endif/;}'; }
{
  echo "static const int CAT21[] = {"; sed 's/$/,/' "$work/ops" | guard; echo "};"
  echo "static const char *const CAT21_NOME[] = {"; sed 's/.*/"&",/' "$work/ops" | guard; echo "};"
  # Rows of every build, as written (platform rows sit behind #ifdef).
  echo "static const char *const TELA_FONTE[] = {"
  grep -oE 'OPCA?\(AJ_[A-Z0-9_]*' src/ajustes_ux_tela.inc | sed -E 's/OPCA?\((.*)/"\1",/'; echo "};"
} > "$work/cat21.h"

# Removed options: neither the key nor its getter may be read outside Settings.
fail=0
for name in AJ_DESCOBRIR ajustes_local_descobrir ajustes_descobrir_na_busca \
            AJ_DET_TRAILER_SOM AJ_RELOGIO_POS ajustes_relogio_pos AJ_HDR_INTERFACE \
            AJ_PROF_BORDA AJ_PROF_COBERTURA AJ_PROF_POSTERS AJ_PROF_CW AJ_PROF_EPS \
            AJ_PROF_ELENCO AJ_PROF_TRAILERS ajustes_profundidade_borda ajustes_profundidade_cobertura; do
  users="$(grep -lw "$name" src/*.c src/*.h src/*.inc src/*/*.c 2>/dev/null |
           grep -v '^src/ajustes' | grep -v '^src/idioma' || true)"
  if [ -n "$users" ]; then echo "FAIL: removed $name is still read by: $users"; fail=1; fi
done
grep -q 'int ajustes_hdr_interface(void) { return 0; }' src/ajustes.c ||
  { echo "FAIL: AJ_HDR_INTERFACE is listed as removed but its getter is live"; fail=1; }

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ajustes_catalogo21.c -Isrc -I"$work" -o "$work/t" -DAJUSTES_TESTE \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/t" || fail=1
exit $fail
