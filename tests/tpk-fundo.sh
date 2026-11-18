#!/bin/bash
# No TV: leaving Nuvio during playback (Home, TV sleep) and coming back.
#
# The host stops the player on background (#412) and says where it was; the
# session must stay suspended at that exact point, send nothing to the dead
# player, and on return reopen the same source muted, seek exactly, pause, give
# the sound back and restore the tracks. If it can't, the player screen closes.
# Runs video_tpk.c against a fake host; the 20 s give-up is shortened.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DRET_PRAZO_MS=1500u -DRET_PAUSA_MS=800u -DRET_BUSCA_MS=800u -Isrc tests/tpk-fundo.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c tests/capmkv_stub.c tests/dts_tpk_stub.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-fundo"
"${TMPDIR:-/tmp}/nuvio-tpk-fundo"
