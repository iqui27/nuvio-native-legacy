#!/bin/bash
# A fila do decodificador do navegador (src/webp.c + tools/decodificador.js)
# com prazo curto e dois fios concorrentes, no Node, com pthreads do
# Emscripten de verdade e o decodificador.js REAL (so o codec e falso, ver
# tests/decodefila-shim.js). Duas rodadas: Worker vivo, e Worker que morre
# logo depois de subir (reencaminhamento ao fio principal).
#
# Com o protocolo da 1.4.1 (Worker em Atomics.wait pelo malloc do C) a rodada
# viva dava `nulos_rapidos=44`: pedidos de 5 ms perdidos atras dos 8 s que o
# Worker esperava por pedidos ja abandonados — a cascata dos logs da Samsung.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
EMCC="$EMSDK_DIR/upstream/emscripten/emcc"
[ -x "$EMCC" ] || { echo "decodefila-tizen.sh: emcc nao encontrado em $EMSDK_DIR" >&2; exit 127; }
mkdir -p build/decodefila
"$EMCC" tests/decodefila_tizen.c src/webp.c -O1 -DNV_NAV_PRAZO_MS=300 \
  -pthread -sPTHREAD_POOL_SIZE=3 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=0 \
  -sENVIRONMENT=node -sEXIT_RUNTIME=0 -sUSE_SDL=2 -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  --pre-js tests/decodefila-shim.js -o build/decodefila/teste.js
# Tres rodadas: Worker vivo; Worker morto (reencaminhamento ao fio
# principal); e Worker vivo com o FIO PRINCIPAL OCUPADO em blocos de 1 s (as
# tarefas longas da Samsung 1.4.1). A terceira e a do canal direto: pela ponte
# antiga (via fio principal) ela dava nulos_rapidos=35 e max_ms_rapido=2000.
ok=0
for rodada in "0 0" "1 0" "0 1000"; do
  set -- $rodada
  log=build/decodefila/morto$1-ocupado$2.log
  if NV_RAIZ="$PWD" NV_SHIM_MORTO=$1 NV_SHIM_OCUPADO=$2 timeout 120 node build/decodefila/teste.js >"$log" 2>&1; then
    echo "worker_morto=$1 fio_principal_ocupado=${2}ms $(grep RESULTADO "$log")"
  else
    echo "FALHOU worker_morto=$1 fio_principal_ocupado=${2}ms"; tail -20 "$log"; ok=1
  fi
done
exit $ok
