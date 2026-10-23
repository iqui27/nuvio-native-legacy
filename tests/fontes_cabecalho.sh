#!/bin/bash
# Sources sheet: no dark band under the header over a scrolled list. Not in the suite (GL window).
#   bash tests/fontes_cabecalho.sh <dir> [git-ref-of-old-streams.c] [fade|clip]
# Optional mutations must fail the pixel assertions (regression proof).
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/rev/fontecab/after}"
mkdir -p "$out" "$D/data"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  if [ "$source" = src/streams.c ]; then
    if [ -n "${2:-}" ]; then git show "$2:src/streams.c" > "$D/streams.c"
    else cp "$source" "$D/streams.c"; fi
    # Same measurement point on both revisions; no revision-specific shot hooks.
    python3 - "$D/streams.c" "${3:-}" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1])
s = p.read_text()
if sys.argv[2] == "clip":
    s = s.replace("(rolagem > 1.0f ? 0.0f : 8.0f)", "8.0f")
if sys.argv[2] == "fade":
    s = s.replace("  // A FONTE DOS DADOS,",
        "  if (rolagem > 1.0f) gfx_rect((GfxRect){x,FOLHA_TOPO-8,w,34},"
        "0,GFX_BRILHO_TOPO,0,1.0f,0,0,.071f,.075f,.086f,"
        "(ajustes_vidro()?.78f:.98f)*anim);\n  // A FONTE DOS DADOS,", 1)
needle = "  nf=nOrdem;\n"
head, tail = s.rsplit(needle, 1)
p.write_text(head + needle +
             "  extern void fontes_painel(float, float, float);\n"
             "  fontes_painel(FOLHA_TOPO, x, w);\n" + tail.replace(
                 "  // A FONTE DOS DADOS,", "  extern void fontes_conferir(void);\n"
                 "  fontes_conferir();\n  // A FONTE DOS DADOS,", 1))
PY
    source="$D/streams.c"
  fi
  sources+=("$source")
done
bin="$D/test"
flags=(-DNV_SHOT_HOOKS -Isrc -O0 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
objs=()
for source in "${sources[@]}" tests/fontes_cabecalho.c; do objs+=("$D/$(echo "$source" | tr '/' '_').o"); done
printf '%s\n' "${sources[@]}" tests/fontes_cabecalho.c |
  xargs -P 8 -I{} sh -c 'cc "$@" -c {} -o "$0/$(echo {} | tr / _).o"' "$D" "${flags[@]}"
cc "${objs[@]}" -o "$bin" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
modes=(player detail no-clock player-glass detail-glass)
for mode in "${modes[@]}"; do
  NUVIO_DADOS="$D/data" NUVIO_TESTE_DIR="$D/data" "$bin" "$out/$mode" "$mode"
done
