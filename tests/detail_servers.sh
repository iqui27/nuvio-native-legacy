#!/bin/bash
# Focused native proof; cached module objects keep the regression runs small.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
export OBJ="/Volumes/ExternalSSD/tmp/rev/detsrv/obj"
out=/Volumes/ExternalSSD/tmp/rev/detsrv/after
mkdir -p "$out" "$OBJ"
export NUVIO_DETSRV_PROOF=1 NUVIO_SHOT_SERVIDORES=1 NUVIO_SHOT_AMIGOS=1
export NUVIO_SHOT_IDIOMA=1 NUVIO_SHOT_RELOGIO=1
if [ "${DETSRV_SKIP_SHOTS:-0}" != 1 ]; then
  bash tests/detalhe_glass_shot.sh "$out" detalhe-chip
fi
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
work=$(mktemp -d "$TMPDIR/nv-detsrv.XXXXXX")
trap 'rm -rf "$work"' EXIT
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  objects+=("$OBJ/$(basename "$source" .c).o")
done
cc "${flags[@]}" -c src/detail.c -o "$work/detail.o"
cc "${flags[@]}" tests/detail21.c "${objects[@]}" -o "$work/detail21" "${libs[@]}"
mkdir -p "$work/data"
NUVIO_DADOS="$work/data" "$work/detail21"
for target in biblioteca_origens fontes_lista fontes_anim biblioteca_shot; do
  selected=()
  for source in src/*.c src/dts/*.c; do
    case "$source" in src/main.c|src/detail.c) continue;; esac
    case "$target:$source" in biblioteca_origens:src/biblioteca.c|fontes_anim:src/streams.c) continue;; esac
    selected+=("$OBJ/$(basename "$source" .c).o")
  done
  extra=()
  if [ "$target" = fontes_anim ]; then
    extra=(-DNV_SHOT_HOOKS)
    cc "${flags[@]}" -DNV_SHOT_HOOKS -c src/ajustes.c -o "$work/ajustes.o"
    selected=()
    for source in src/*.c src/dts/*.c; do
      case "$source" in src/main.c|src/detail.c|src/streams.c|src/ajustes.c) continue;; esac
      selected+=("$OBJ/$(basename "$source" .c).o")
    done
    selected+=("$work/ajustes.o")
  fi
  cc "${flags[@]}" ${extra[@]+"${extra[@]}"} tests/"$target".c "$work/detail.o" "${selected[@]}" -o "$work/$target" "${libs[@]}"
  mkdir -p "$work/$target-data"
  case "$target" in
    fontes_anim)
      for mode in warm cold full reduced solid; do
        NUVIO_DADOS="$work/$target-data" NUVIO_TESTE_DIR="$work/$target-data" "$work/$target" "$out" "$mode"
      done;;
    biblioteca_shot)
      NUVIO_DADOS="$work/$target-data" "$work/$target" "$out/library" servers
      if [ -n "${DETSRV_BEFORE_REV:-}" ]; then
        git show "$DETSRV_BEFORE_REV:src/biblioteca.c" > "$work/biblioteca-before.c"
        cc "${flags[@]}" -c "$work/biblioteca-before.c" -o "$work/biblioteca-before.o"
        before=()
        for source in src/*.c src/dts/*.c; do
          case "$source" in src/main.c|src/detail.c|src/biblioteca.c) continue;; esac
          before+=("$OBJ/$(basename "$source" .c).o")
        done
        cc "${flags[@]}" tests/biblioteca_shot.c "$work/detail.o" "$work/biblioteca-before.o" "${before[@]}" -o "$work/library-before" "${libs[@]}"
        mkdir -p "$work/library-before-data"
        NUVIO_DADOS="$work/library-before-data" "$work/library-before" "${out%/after}/before/library" servers
      fi;;
    *) NUVIO_DADOS="$work/$target-data" "$work/$target";;
  esac
done
bash tests/plex.sh
bash tests/jellyfin.sh
