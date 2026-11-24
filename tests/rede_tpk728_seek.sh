#!/bin/bash
# #728: seek do usuario durante a reconexao do TPK sem convertido aponta o
# "reabrindo" para o alvo novo (ver tests/rede_tpk728_seek.c). Host e resolver
# falsos; sem rede, sem GL, sem aparelho.
#   SANITIZE=1 ASan+UBSan, SANITIZE=thread TSan, SANITIZE=0 nenhum.
set -eu
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
for flag in '' '-DNV_TPK40'; do
  ${CC:-cc} -O1 -g -Wall -DNV_TPK $flag -DNV_TEST_DIRECT_CDN -Isrc ${flags[@]+"${flags[@]}"} tests/rede_tpk728_seek.c src/video_tpk.c src/mkvfix.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c \
    src/farolerro.c src/farolanel.c src/fontesonda.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk728-seek"
  "${TMPDIR:-/tmp}/nuvio-tpk728-seek"
done
