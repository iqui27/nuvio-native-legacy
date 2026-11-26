#!/bin/bash
# No TV: burn-in with the original first, the switch, the slow-start notice
# and the stuck item (owner 24/11/2026). Fake host from tpk-fundo.c.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DTB_LENTO_MS=600u -DRET_PRAZO_MS=6000u -Isrc tests/tpk-tbqueima.c src/video_tpk.c src/tpk45.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c src/audioext.c src/dts/doador_tpk.c tests/capmkv_stub.c tests/dts_tpk_stub.c tests/tbburn_stub.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-tbqueima"
"${TMPDIR:-/tmp}/nuvio-tpk-tbqueima"
