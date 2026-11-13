#!/bin/bash
# Batch E source-list regressions: hidden GL window, offline, UBSan + Guard Malloc.
# (ASan hangs at startup on this host whenever SDL2 is linked; libgmalloc puts a
#  guard page before/after every heap block, which is what the neighbour reads hit.)
#   bash tests/fontes_lote_e.sh            current tree (must pass)
#   bash tests/fontes_lote_e.sh --mutant   neighbour bounds guard removed (must fail)
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-lote-e.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/data" "$work/obj" "$work/src"
san=(-fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
flags=(-DNV_SHOT_HOOKS -DNV_ADDON_LAYOUT_TEST -Isrc -O1 -g "${san[@]}"
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
test_src=tests/fontes_lote_e.c
if [ "${1:-}" = --mutant ]; then
  sed 's/if (row < 0 || row >= nOrdem || ordem\[row\] < 0 || ordem\[row\] >= n) continue;/if (0) continue;/' \
    src/streams.c > "$work/src/streams.c"
  ! cmp -s src/streams.c "$work/src/streams.c" || { echo "mutant: guard not found"; exit 2; }
  sed 's|"../src/streams.c"|"streams.c"|' tests/fontes_lote_e.c > "$work/src/fontes_lote_e.c"
  flags+=(-I"$work/src"); test_src=$work/src/fontes_lote_e.c
fi
export NV_OBJ="$work/obj"
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" "$test_src" |
  xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_OBJ/$(basename "$0").o"' '{}' "${flags[@]}"
cc "${san[@]}" "$work/obj/"*.o -o "$work/test" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
# Bounded run (fio1-pilha.sh convention; macOS has no coreutils timeout): a
# startup hang dies by SIGALRM instead of stalling the job. env sets DYLD_*
# after SIP strips them from the protected perl/env binaries.
perl -e 'alarm shift; exec @ARGV' 300 env DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib \
  MALLOC_PROTECT_BEFORE=1 MALLOC_STRICT_SIZE=1 \
  NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test"
