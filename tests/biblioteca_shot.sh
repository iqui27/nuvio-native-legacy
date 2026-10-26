#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da Biblioteca renovada, em BMP, sem interacao e sem rede.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/biblioteca_shot.sh /tmp/nuvio-biblioteca
#
# NUVIO_DADOS e uma pasta temporaria e o programa recusa rodar se dados_dir()
# nao for ela: fixar uma lista GRAVA.
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nuvio-biblioteca}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibshot-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" = src/main.c ]; then continue; fi
  if [ "${NV_J1_SHOT:-}" = 1 ] && [ "$source" = src/biblioteca.c ]; then continue; fi
  sources+=("$source")
done
mkdir -p "$(dirname "$saida")"
compiler=(cc)
if command -v ccache >/dev/null; then
  export CCACHE_DIR="${CCACHE_DIR:-${TMPDIR:-/tmp}/nuvio-biblioteca-ccache}"
  compiler=(ccache cc)
fi
objects=()
shot_flags=()
if [ "${NV_J1_SHOT:-}" = 1 ]; then shot_flags+=(-DNV_J1_SHOT); fi
if [ -n "${BIB_REV:-}" ]; then
  git show "$BIB_REV:src/biblioteca.c" > "$tmp/biblioteca.c"
  shot_flags+=("-DBIB_FONTE=\"$tmp/biblioteca.c\"" -DNV_BIB_BEFORE)
fi
for source in "${sources[@]}" tests/biblioteca_shot.c; do
  object="$tmp/$(basename "$source" .c).o"
  "${compiler[@]}" -c "$source" -Isrc -o "$object" \
    "${shot_flags[@]}" -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
  objects+=("$object")
done
cc "${objects[@]}" -o "$tmp/shot" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
if [ "${NV_MODO:-}" = mock ] || [ "${NV_MODO:-}" = servers ]; then
  export NUVIO_RAIL=moderna NUVIO_SHOT_RELOGIO=1 NUVIO_SHOT_PT=1
fi
if [ "${NV_J1_SHOT:-}" = 1 ]; then
  mkdir -p "$saida"
  cp "$tmp/shot" "$saida/capture"
  # Compile once; the focused matrix covers the owner's layouts and languages.
  if [ "${NV_BIB_MATRIX:-}" = 1 ]; then
    export NV_J1_STILLS=1 NV_BIB_SO_GRADE=1 NUVIO_SHOT_RELOGIO=1 NUVIO_SHOT_SYNC_MIN=3
    for lang in pt en; do
      for rail in dinamica moderna; do
        for scale in 1 .85; do
          dest="$saida/$lang-$rail-$scale"
          mkdir -p "$dest"
          unset NUVIO_SHOT_PT NUVIO_SHOT_EN
          if [ "$lang" = en ]; then export NUVIO_SHOT_EN=1; else export NUVIO_SHOT_PT=1; fi
          NUVIO_RAIL="$rail" NUVIO_SHOT_ESCALA_TEXTO="$scale" NUVIO_SHOT_VIDRO=0 \
          NUVIO_DADOS="$tmp/dados-$lang-$rail-$scale" "$tmp/shot" "$dest/native" menu
        done
      done
    done
    exit 0
  fi
  # NUVIO_SHOT_EN=1 captures the English UI; otherwise pt-BR.
  if [ -z "${NUVIO_SHOT_EN:-}" ]; then export NUVIO_SHOT_PT=1; fi
  for scale in ${NV_J1_SCALES:-1 .85 .9 1.1 1.2 1.3}; do
    for glass in ${NV_J1_GLASS:-1 0}; do
      for mode in ${NV_J1_MODES:-mock servers}; do
        mkdir -p "$saida/$scale-$glass/$mode"
        NUVIO_RAIL="${NUVIO_RAIL:-moderna}" NUVIO_SHOT_RELOGIO=1 \
        NUVIO_SHOT_ESCALA_TEXTO="$scale" NUVIO_SHOT_VIDRO="$glass" \
        NUVIO_DADOS="$tmp/dados-$scale-$glass-$mode" "$tmp/shot" "$saida/$scale-$glass/$mode/native" "$mode"
      done
    done
  done
  exit 0
fi
NUVIO_DADOS="$tmp/dados" "$tmp/shot" "$saida" "${NV_MODO:-}"
if [ "${NV_MODO:-}" = mock ] || [ "${NV_MODO:-}" = servers ]; then exit 0; fi
# SEGUNDA VOLTA so para a MEDIDA: comeca em lista, com o cache de textura frio.
# Ver a nota em tests/biblioteca_shot.c sobre por que uma volta so nao mede.
echo "--- segunda volta: lista com o cache frio ---"
NUVIO_DADOS="$tmp/dados2" "$tmp/shot" "$tmp/descarte" lista | grep "\[arte\]"
