#!/bin/bash
# Custo de desenho do player por estado, sem e com vidro. Ver
# tests/fluidez_perf_player.c. Nao entra na suite: precisa de janela GL e o
# numero e para comparar dois commits, nao para passar/falhar.
#
#   bash tests/fluidez_perf_player.sh                  # estados padrao
#   bash tests/fluidez_perf_player.sh osd fontes       # so estes (nomes de player_glass_shot.c)
set -euo pipefail
cd "$(dirname "$0")/.."
estados=("$@")
if [ ${#estados[@]} -eq 0 ]; then estados=(osd osd-barra legendas fontes pausa episodios aovivo-osd); fi
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fluidez-perf-player-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for s in src/*.c src/dts/*.c; do [ "$s" != src/main.c ] && sources+=("$s"); done
cc -DNV_SHOT_HOOKS -DNV_FLUIDEZ_PERF "${sources[@]}" tests/fluidez_perf_player.c -Isrc -Itests \
  -o "$tmp/perf" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
printf '%-12s %-6s %6s %6s %6s %6s %6s %6s\n' estado vidro fill vis rects progs binds cheios
for e in "${estados[@]}"; do
  for v in 0 1; do
    mkdir -p "$tmp/dados" "$tmp/caps"
    NUVIO_SHOT_VIDRO=$v NUVIO_DADOS="$tmp/dados" NUVIO_TESTE_DIR="$tmp/dados" \
      "$tmp/perf" "$tmp/caps" "$e" 2>/dev/null | grep '^Q ' | tail -60 | awk -v e="$e" -v v="$v" '
        { for (i = 2; i <= NF; i++) { split($i, a, "="); s[a[1]] += a[2] } n++ }
        END { if (!n) { printf "%-12s %-6s sem quadros (estado desconhecido?)\n", e, v; exit }
              printf "%-12s %-6s %6.2f %6.2f %6.0f %6.0f %6.0f %6.1f\n", e, v,
                     s["fill"]/n, s["vis"]/n, s["rects"]/n, s["progs"]/n, s["binds"]/n, s["cheios"]/n }'
    rm -rf "$tmp/dados" "$tmp/caps"
  done
done
