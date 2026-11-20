#!/bin/bash
# Canais da Live TV no Spotlight com um guia de exemplo (addon falso local).
# Ver o cabecalho de tests/spotlight_canais.c. Precisa de python3; sem janela.
#
#   bash tests/spotlight_canais.sh
set -euo pipefail
cd "$(dirname "$0")/.."
PORTA=8766
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spot-canais-XXXXXX")"
NUVIO_DADOS="$tmp/dados"; mkdir -p "$NUVIO_DADOS"; export NUVIO_DADOS
python3 tests/spotlight_canais_servidor.py $PORTA 2>"$tmp/servidor.log" & SRV=$!
trap 'kill $SRV 2>/dev/null || true; rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = "src/main.c" ] && continue
  sources+=("$source")
done
cc "${sources[@]}" tests/spotlight_canais.c -Isrc -o "$tmp/t" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
sleep 0.3
"$tmp/t" rede "http://127.0.0.1:$PORTA" | grep -E '^(spotlight|guia|fonte|PASS)|\[guia\]|\[spotlight\]'
grep -c 'GET /catalog/' "$tmp/servidor.log" >/dev/null
kill $SRV; wait $SRV 2>/dev/null || true
# segundo processo: servidor fora, guia nunca carregado
"$tmp/t" cache "http://127.0.0.1:$PORTA" | grep -E '^(spotlight|PASS)|\[guia\] cache'
echo "spotlight_canais: ok"
