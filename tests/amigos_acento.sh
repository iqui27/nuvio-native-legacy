#!/bin/bash
# Real GL regression; isolated baseline proves the solid focus failure first.
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${R:-${TMPDIR:-/tmp}}/amigosacento}"
mkdir -p "$saida/before" "$saida/after"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-amigos-acento.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/base/tests" "$work/obj"
git archive 6b86b60feedb43f320b76a464913736a2b167766 src | tar -x -C "$work/base"
cp tests/amigos_acento.c "$work/base/tests/"
flags=(-Dtime=accent_time -DSDL_GetTicks=accent_ticks -DAJUSTES_TESTE -DNV_SOCIALVIS_DEMO
  '-DNV_REC_URL="http://127.0.0.1:1"' -O1 -g -Itests
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
for stage in before after; do
  sources="$work/base/src"
  harness="$work/base/tests/amigos_acento.c"
  if [ "$stage" = after ]; then sources=src; harness=tests/amigos_acento.c; fi
  objs=()
  for source in "$sources"/*.c "$sources"/dts/*.c; do
    case "$source" in */main.c|*/ajustes.c) continue;; esac
    relative="${source#"$sources"/}"
    o="$work/obj/${relative//\//_}.o"
    objs+=("$o")
    extra=()
    case "$source" in */recomenda.c) extra=(-Drecomenda_contatos=accent_real_contacts);; esac
    cc "${flags[@]}" -I"$sources" "${extra[@]}" -c "$source" -o "$o"
  done
  cc "${flags[@]}" -I"$sources" "${objs[@]}" "$harness" -o "$work/test" \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
  export NUVIO_DADOS="$work/dados-$stage"
  mkdir -p "$NUVIO_DADOS"
  if [ "$stage" = before ]; then
    set +e
    "$work/test" "$saida/before" > "$saida/before.log" 2>&1
    rc=$?
    set -e
    if [ "$rc" -ne 134 ] || ! /usr/bin/grep -q 'Assertion failed: (p\[0\] > p\[1\] + 12' "$saida/before.log"; then
      tail -20 "$saida/before.log"
      echo "Expected baseline failure on solid accent pixels; got rc=$rc" >&2
      exit 1
    fi
    echo "Baseline: expected solid accent pixel assertion failed (rc=$rc)"
  else
    "$work/test" "$saida/after"
    "$work/test" --compare "$saida/before" "$saida/after"
  fi
done
