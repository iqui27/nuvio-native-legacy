#!/bin/bash
# No TV: the converted stream reopening on seek and reconnect (see the top of
# tests/tpk-dts-reabrir.c). Fake host, fake converter, hand-built MKV
# header; no network, no GL, no device.
#   SANITIZE=1 ASan+UBSan, SANITIZE=thread TSan, SANITIZE=0 nenhum.
set -eu
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
rc=0
for flag in '' '-DNV_TPK40'; do
  ${CC:-cc} -O1 -g -Wall -DNV_TPK $flag -Isrc ${flags[@]+"${flags[@]}"} tests/tpk-dts-reabrir.c src/video_tpk.c src/tpk45.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c \
    src/farolerro.c src/farolanel.c src/fontesonda.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-dts-reabrir"
  "${TMPDIR:-/tmp}/nuvio-tpk-dts-reabrir" || rc=1
done
exit $rc
