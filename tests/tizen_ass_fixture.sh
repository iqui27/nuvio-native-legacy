#!/bin/bash
# Gera um MKV ASS pequeno e deterministico para o emulador/TV Tizen.
# Nao baixa midia, nao usa credenciais e so serve a pasta indicada.
set -eu
cd "$(dirname "$0")/.."
OUT=${1:-/tmp/nuvio-tizen-ass}
MODO=${2:-}
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || FFMPEG=$(command -v ffmpeg || true)
[ -n "$FFMPEG" ] && [ -x "$FFMPEG" ] || { echo "ffmpeg nao encontrado (defina FFMPEG)" >&2; exit 2; }
mkdir -p "$OUT"
cp tests/fixtures/ass/tizen.ass "$OUT/tizen.ass"
"$FFMPEG" -v error -y \
  -f lavfi -i "color=c=0x152238:s=640x360:r=24:d=9" \
  -f lavfi -i "sine=frequency=440:duration=9" \
  -i "$OUT/tizen.ass" \
  -map 0:v -map 1:a -map 2:s -shortest \
  -c:v libx264 -preset ultrafast -tune zerolatency -g 24 -pix_fmt yuv420p \
  -c:a aac -c:s ass -map_metadata -1 -fflags +bitexact \
  -flags:v +bitexact -flags:a +bitexact -metadata title="Nuvio local ASS fixture" \
  "$OUT/tizen-ass.mkv"
shasum -a 256 "$OUT/tizen.ass" "$OUT/tizen-ass.mkv"
cat <<EOF
Fixture pronta, somente local:
  ASS: $OUT/tizen.ass
  MKV: $OUT/tizen-ass.mkv
  URL esperada: http://<IP_DO_HOST>:<PORTA>/tizen-ass.mkv

Caso manual no emulador/TV:
  1. Selecione a faixa embutida ASS no player; confirme duas falas sobrepostas
     (embaixo + topo) e a fala com \\N em duas linhas.
  2. Avance para 00:00:06 e volte para 00:00:02; a faixa deve continuar
     sincronizada e sem duplicar a sessao.
  3. Troque para Nenhuma e depois para ASS; confirme que o overlay anterior
     some antes de a nova faixa aparecer.
EOF
if [ "$MODO" = "--serve" ]; then
  exec python3 tests/servidor_range.py "$OUT"
fi
