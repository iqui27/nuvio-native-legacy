#!/bin/bash
# Compila o WGT de diagnóstico ASS, separado do pacote NuvioTV002.
#
# O vídeo e a legenda ficam fora do pacote e são servidos por
# tools/tizen-ass-server.py. A URL nunca é escrita em log pelo harness; ela só
# entra no shell gerado para o emulador buscar a fixture conhecida.
set -euo pipefail
cd "$(dirname "$0")/.."

FIXTURE="${NUVIO_ASS_FIXTURE:-/tmp/nuvio-tizen-ass-1312-1790022235}"
BASE="${NUVIO_ASS_BASE_URL:-}"
SAIDA="${NUVIO_SAIDA:-build/tizen-ass-diagnostic}"
NOME="${NUVIO_WGT_NOME:-NuvioTV-ass-diagnostic-0.1.0}"
ESTAGIO="${NUVIO_WGT_ESTAGIO:-build/wgt-stage-ass}"

[ -s "$FIXTURE/tizen-ass.mkv" ] || { echo "teste-tizen-ass: fixture MKV ausente: $FIXTURE/tizen-ass.mkv" >&2; exit 2; }
[ -s "$FIXTURE/tizen.ass" ] || { echo "teste-tizen-ass: fixture ASS ausente: $FIXTURE/tizen.ass" >&2; exit 2; }
[ -n "$BASE" ] || {
  echo "teste-tizen-ass: defina NUVIO_ASS_BASE_URL (servidor Range/CORS da fixture)" >&2
  exit 2
}

VIDEO_URL="$BASE/tizen-ass.mkv"
ASS_URL="$BASE/tizen.ass"

# Confere que a mesma origem que será usada pelo AVPlay responde ao Range e a
# mesma que a legenda usa responde CORS. O corpo não é guardado nem exibido.
curl -fsSI "$VIDEO_URL" >/dev/null
curl -fsS -H 'Origin: file://' -H 'Range: bytes=0-31' -D - -o /dev/null "$VIDEO_URL" |
  grep -qi '^Access-Control-Allow-Origin:'
curl -fsS -H 'Origin: file://' -D - -o /dev/null "$ASS_URL" |
  grep -qi '^Access-Control-Allow-Origin:'

mkdir -p "$SAIDA"
python3 - "tools/tizen-ass-shell.html" "$SAIDA/tizen-ass-shell.html" "$VIDEO_URL" "$ASS_URL" "$BASE/controle" <<'PY'
import json
import pathlib
import sys

template = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
template = template.replace("@ASS_VIDEO_URL@", json.dumps(sys.argv[3]))
template = template.replace("@ASS_TEXT_URL@", json.dumps(sys.argv[4]))
template = template.replace("@ASS_CONTROL_URL@", json.dumps(sys.argv[5]))
pathlib.Path(sys.argv[2]).write_text(template, encoding="utf-8")
PY

export NUVIO_SAIDA="$SAIDA"
export NUVIO_TIZEN_SHELL="$SAIDA/tizen-ass-shell.html"
export NUVIO_TIZEN_EXTRA_SOURCES="tools/teste-tizen-ass.c"
export NUVIO_TIZEN_EXCLUDE_MAIN=1
export NUVIO_TIZEN_CONFIG="tools/tizen-ass-config.xml"
export NUVIO_WGT_ESTAGIO="$ESTAGIO"
export NUVIO_WGT_NOME="$NOME"
# Este WGT e um harness separado do app principal e nao precisa de login. O
# opt-out e deliberado para nao transformar um teste AVPlay local em release.
export NUVIO_TIZEN_DIAGNOSTIC=1
# O harness não precisa do coletor de produção e não deve herdar credenciais de
# diagnóstico de uma sessão do shell pai.
unset NUVIO_LOG_URL NUVIO_DIAG_TOKEN

bash tools/tizen.sh
NUVIO_WGT_NOME="$NOME" NUVIO_WGT_ESTAGIO="$ESTAGIO" \
  NUVIO_TIZEN_CONFIG="tools/tizen-ass-config.xml" bash tools/tizen-wgt.sh

echo "teste-tizen-ass: pacote sem assinatura: $NOME.wgt"
sha256sum "$NOME.wgt" "$FIXTURE/tizen.ass" "$FIXTURE/tizen-ass.mkv"
echo "teste-tizen-ass: package=NuvioASS01 app=NuvioASS01.AssDiag"
echo "teste-tizen-ass: assine no Certificate Manager autorizado e instale o WGT assinado no emulador; o app inicia o ciclo sozinho"
