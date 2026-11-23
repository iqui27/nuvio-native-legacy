#!/bin/bash
# Saida do player no .tpk: as faixas saem junto e os callbacks tardios do host
# nao repovoam nada (PR 734, parte A). video_tpk.c entra no TU do teste para
# conferir os estaticos; o resto e a mesma ponte de tpk-escolha.sh.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DLEG_SEM_CUE_MS=1000u -Isrc tests/tpk_faixas_saida.c src/video_modo.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c tests/capmkv_stub.c tests/dts_tpk_stub.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-faixas-saida"
"${TMPDIR:-/tmp}/nuvio-tpk-faixas-saida"
