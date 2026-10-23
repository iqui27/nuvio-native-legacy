#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da pagina do titulo no Glass UI, para comparar com os mockups
# (design/glass-ilha "Detalhe" e o "detalhe-retomar" do player). Nao entra na
# suite (testa-tudo.sh pula *_shot.sh): precisa de janela GL e de olho.
#
#   bash tests/detalhe_glass_shot.sh <pasta-saida> [id...]
#   NUVIO_SHOT_VIDRO=1 para o material vidro (sem ele, solido).
#
# Os .o dos outros modulos ficam em cache (/tmp/nuvio-detglass-obj) e so
# recompilam quando o .c muda; detail.c entra pelo include do teste.
set -eu
cd "$(dirname "$0")/.."
if [ -n "${NUVIO_DETAIL_TIER:-}" ]; then
  echo 'device tiers require a device GLES runner; use NUVIO_DETAIL_CONFIG for host checks' >&2
  exit 2
fi
OBJ=${OBJ:-${TMPDIR:-/tmp}/nuvio-detglass-obj}
if [ $# -eq 0 ]; then set -- "$TMPDIR/nuvio-detglass-shots"; fi
mkdir -p "$OBJ" "$1"
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-detglass-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
FL="-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined"
if [ -n "${NUVIO_C9_CACHE:-}" ]; then FL="$FL -DNV_SHOT_HOOKS"; fi
objs=()
pids=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c|src/detail.c) continue;; esac
  extra=""
  o="$OBJ/$(basename "$s" .c).o"
  # Count the shared wrapping helper too, without production instrumentation.
  if [ "$s" = src/agendaui.c ] && [ -n "${NUVIO_TRAKT_CHECK:-}" ]; then
    o="$OBJ/agendaui-trakt.o"
    extra="-Dtxt_largura=trakt_largura -Dtxt_token_tam=trakt_token_tam"
  fi
  if [ "$s" = src/notasui.c ] && [ "${NUVIO_DETAIL_CHECK:-}" = b2 ]; then
    o="$OBJ/notasui-b2.o"
    extra="-Dgfx_vidro_painel=b2_panel -Dgfx_luz_canto=b2_tint -Dgfx_cor=b2_color -Dtxt_desenhar_alpha=b2_text -Dtxt_bloco_corta=b2_block"
    if [ -n "${NUVIO_NOTAS_SOURCE:-}" ]; then
      s="$NUVIO_NOTAS_SOURCE"
      o="$OBJ/notasui-b2-before.o"
    fi
  fi
  # NUVIO_DETAIL_CHECK=mais: spy on the glass density plrui hands to gfx.
  if [ "$s" = src/plrui.c ] && [ "${NUVIO_DETAIL_CHECK:-}" = mais ]; then
    o="$OBJ/plrui-spy.o"
    extra="-Dgfx_vidro_material=spy_vidro_material"
  fi
  if [ "$s" = src/detmais.c ] && [ -n "${NUVIO_DETMAIS_SOURCE:-}" ]; then
    s="$NUVIO_DETMAIS_SOURCE"
    o="$OBJ/detmais-before.o"
  fi
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
    cc $FL $extra -c "$s" -o "$o" & pids+=($!)
    if [ ${#pids[@]} -ge 8 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  fi
done
wait
shot_extra=()
if [ -n "${NUVIO_DETAIL_SOURCE:-}" ]; then
  shot_extra+=("-DNV_DETAIL_SOURCE=\"$NUVIO_DETAIL_SOURCE\"")
fi
cc $FL "${shot_extra[@]}" tests/detalhe_glass_shot.c "${objs[@]}" -o "$OBJ/shot" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$OBJ/shot" "$@"
