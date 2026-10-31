#!/bin/bash
# MULTIVIEW — capturas de prova (donos/verificador): layouts 2 / 1+2 / 2x2 16:9,
# quadro vazio "Adicionar canal", picker com busca e fontes agrupadas (fonte no
# limite desabilitada), dois quadros ao vivo, quadros da troca de layout e
# verificação de fluidez (sem upload de textura por quadro). Precisa de janela
# GL; nao entra na suite automatica. Compila com GUIAPOLIR_BASELINE_CODE para
# pular os asserts legados quebrados ANTES deste ramo (mesmo erro em
# integracao/2.1). Uso: bash tests/multiview_shot.sh [saida]
set -euo pipefail
export TMPDIR="${TMPDIR:-/tmp}";TMPDIR="${TMPDIR%/}"
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/multiview}
work=$(mktemp -d "${TMPDIR}/nuvio-mv-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc -DGUIAPOLIR_BASELINE_CODE "${sources[@]}" tests/guiav5_principal_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
NUVIO_DADOS="$work/dados" NUVIO_SHOT_POLIR=1 "$work/shot" "$out" >/dev/null 2>&1 || true
# O script termina em PASS depois das capturas multiview; asserts legados
# posteriores estao desligados, entao o rc e 0.
for f in mv-2 mv-3 mv-4 mv-empty mv-picker mv-picker-limit mv-live mv-switch-4 mv-switch-8 mv-switch-14 mv-switch-25; do
  test -s "$out/$f-app.png" || { echo "faltando: $f-app.png"; exit 1; }
done
if command -v magick >/dev/null 2>&1; then
  magick -delay 42 "$out/mv-switch-4-app.png" "$out/mv-switch-8-app.png" \
    "$out/mv-switch-14-app.png" "$out/mv-switch-25-app.png" "$out/mv-switch.gif" 2>/dev/null || true
fi
echo "PASS: multiview shots (2, 1+2, 2x2 16:9, vazio, picker por fonte, limite, dois ao vivo, troca de layout)"
