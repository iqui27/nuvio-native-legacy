#!/bin/bash
# Custo de desenho do Guia de TV por estado, sem e com vidro. Ver
# tests/fluidez_perf_guia.c. Nao entra na suite: precisa de janela GL e o
# numero e para comparar dois commits, nao para passar/falhar.
#
#   bash tests/fluidez_perf_guia.sh                 # esta arvore
#   bash tests/fluidez_perf_guia.sh /outra/arvore   # ex.: worktree da v1.7.4
set -euo pipefail
aqui="$(cd "$(dirname "$0")/.." && pwd)"
raiz="${1:-$aqui}"
cd "$raiz"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fluidez-perf-guia-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if ! grep -q gfx_n_cheio_mistura src/gfx.h; then flags+=(-DNV_PERF_BASE); fi
sources=()
for s in src/*.c src/dts/*.c; do case "$s" in src/main.c|src/guia.c) continue;; esac; sources+=("$s"); done
# O guia_shot.c e o wrapper vem desta arvore ($aqui); o codigo, de $raiz. Na
# arvore antiga o guia_shot.c dela e o que vale.
guia_shot="$raiz/tests/guia_shot.c"
[ -f "$guia_shot" ] || { echo "sem tests/guia_shot.c em $raiz" >&2; exit 1; }
cp "$aqui/tests/fluidez_perf_guia.c" "$tmp/fluidez_perf_guia.c"
cc ${flags[@]+"${flags[@]}"} -DNV_FLUIDEZ_PERF "${sources[@]}" "$tmp/fluidez_perf_guia.c" -Isrc -I"$raiz/tests" \
  -o "$tmp/perf" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
printf '%-28s %-6s %6s %6s %6s %6s %6s %6s\n' estado vidro fill vis rects progs binds cheios
for v in 0 1; do
  mkdir -p "$tmp/dados"
  if [ "$v" = 1 ]; then export NUVIO_SHOT_VIDRO=1; else unset NUVIO_SHOT_VIDRO || true; fi
  NUVIO_DADOS="$tmp/dados" "$tmp/perf" "$tmp/caps" 2>/dev/null | awk -v v="$v" '
    /^Q / { n++; for (i = 2; i <= NF; i++) { split($i, a, "="); k = a[1]; q[n, k] = a[2]; ks[k] = 1 } next }
    /^captura: / {
      nome = $2; sub(/.*\//, "", nome)
      ini = n - 29; if (ini < 1) ini = 1
      c = 0; split("", s)
      for (j = ini; j <= n; j++) { c++; for (k in ks) s[k] += q[j, k] }
      if (c) printf "%-28s %-6s %6.2f %6.2f %6.0f %6.0f %6.0f %6.1f\n", nome, v, s["fill"]/c, s["vis"]/c, s["rects"]/c, s["progs"]/c, s["binds"]/c, s["cheios"]/c
      n = 0; split("", q)
    }'
  rm -rf "$tmp/dados"
done
