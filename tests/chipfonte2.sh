#!/bin/bash
# One native target: real parser, renderer, D-pad, baseline failure and PNGs.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/chipfonte2/v2
objects="${TMPDIR:-/tmp}/nv-chipfonte2-$(basename "$PWD")"
mkdir -p "$out" "$objects"
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-chipfonte2.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -DNV_CHIPFONTE2_PROOF -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
export NV_CHIP_OBJECTS="$objects"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  object="$objects/$(basename "$source").o"
  if [[ ! -f "$object" || "$source" -nt "$object" || -n "$(find src -name '*.h' -newer "$object" -print -quit)" ]]; then
    sources+=("$source")
  fi
done
if ((${#sources[@]})); then
  printf '%s\n' "${sources[@]}" | xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_CHIP_OBJECTS/$(basename "$0").o"' '{}' "${flags[@]}"
fi
export NUVIO_CHIPFONTE2_PROOF=1 NUVIO_SHOT_IDIOMA=1 NUVIO_SHOT_TOPO=0 NUVIO_SHOT_SOURCE=1
export NUVIO_SHOT_TIME=1793203200
# Fixed two-chip starting point: later branch movement cannot weaken red proof.
git show 1194556db:src/detail.c > "$work/detail-before.c"
cc "${flags[@]}" "-DNV_DETAIL_SOURCE=\"$work/detail-before.c\"" tests/detalhe_glass_shot.c "$objects/"*.o -o "$work/before" "${libs[@]}"
mkdir -p "$work/nuvio-detglass-dados.before" "$work/nuvio-detglass-dados.red"
NUVIO_DADOS="$work/nuvio-detglass-dados.before" NUVIO_CHIPFONTE2_BEFORE=1 "$work/before" "$out" > "$out/before.log" 2>&1
if NUVIO_DADOS="$work/nuvio-detglass-dados.red" "$work/before" "$work" > "$out/red.log" 2>&1; then
  echo 'FAIL: baseline unexpectedly passed' >&2; exit 1
fi
rg -q 'FAIL: single quality chip: dots=2 source-draws=2' "$out/red.log"
echo 'PASS: two-chip baseline fails the single quality chip regression'
cc "${flags[@]}" tests/detalhe_glass_shot.c "$objects/"*.o -o "$work/after" "${libs[@]}"
mkdir -p "$work/nuvio-detglass-dados.after"
NUVIO_DADOS="$work/nuvio-detglass-dados.after" "$work/after" "$out"
# Existing detail/source regressions, sharing the same production objects.
for target in chipfonte detail21 detail_epcard; do
  cc "${flags[@]}" "tests/$target.c" "$objects/"*.o -o "$work/$target" "${libs[@]}"
  mkdir -p "$work/$target-data"
  NUVIO_DADOS="$work/$target-data" NUVIO_TESTE_DIR="$work/$target-data" "$work/$target"
done
bash tests/detail_acoes_gap.sh
cc "${flags[@]}" -c src/detail.c -o "$work/detail.o"
cc "${flags[@]}" tests/detail_remonta.c "$objects/"*.o "$work/detail.o" -o "$work/detail_remonta" "${libs[@]}"
mkdir -p "$work/detail_remonta-data"
NUVIO_DADOS="$work/detail_remonta-data" "$work/detail_remonta"
selected=()
for object in "$objects/"*.o; do
  [[ "$object" != "$objects/streams.c.o" ]] && selected+=("$object")
done
cc "${flags[@]}" tests/chipfonte_status.c "${selected[@]}" "$work/detail.o" -o "$work/status" "${libs[@]}"
NUVIO_DADOS="$work/chipfonte-data" NUVIO_TESTE_DIR="$work/chipfonte-data" "$work/status"
