#!/bin/bash
# Medida de custo de desenho (home e detalhe) por cenario, no Mac. Ver
# tests/fluidez_perf.c. Nao entra na suite: precisa de janela GL e o numero e
# para comparar dois commits, nao para passar/falhar.
#
#   bash tests/fluidez_perf.sh                # esta arvore
#   bash tests/fluidez_perf.sh /outra/arvore  # ex.: worktree da base
set -euo pipefail
aqui="$(cd "$(dirname "$0")/.." && pwd)"
raiz="${1:-$aqui}"
cd "$raiz"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fluidez-perf-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if ! grep -q gfx_ambiente_descarregar src/gfx.h; then flags+=(-DNV_PERF_BASE); fi
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" "$aqui/tests/fluidez_perf.c" -Isrc -o "$tmp/perf" \
  ${flags[@]+"${flags[@]}"} -DNV_FLUIDEZ_PERF -DNV_SHOT_HOOKS -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O2 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DUMP_FUNDO_DIR="$tmp" NUVIO_DADOS="$tmp/dados" "$tmp/perf" 2>&1 | grep -vE "^\[(tex|arte|cat|desc|home|txt|hero|col|cor|corviva|fileiras|ajustes|dados|gif|perfil|badges|rev)[a-z-]*\]"
