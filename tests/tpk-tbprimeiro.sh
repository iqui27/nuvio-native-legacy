#!/bin/bash
# No TV: TorBox first holds the player for the file's header and opens the
# burned stream at the saved point (owner 18/11). Fake host from tpk-fundo.c.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DTB_CAB_ESPERA_MS=600u -DRET_PRAZO_MS=6000u -Isrc tests/tpk-tbprimeiro.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c tests/capmkv_stub.c tests/dts_tpk_stub.c tests/tbburn_stub.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-tbprimeiro"
"${TMPDIR:-/tmp}/nuvio-tpk-tbprimeiro"
