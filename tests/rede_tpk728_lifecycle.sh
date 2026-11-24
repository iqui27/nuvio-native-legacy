#!/bin/bash
# #728: ciclo de vida da permissao de pouso pausado do "reabrindo". Sonda do
# revisor (evidencias/sol-f60r2/lifecycle.c) adotada como teste: o ack que
# libera capa/busca com o host pausado tem de casar com o alvo da reabertura
# ATUAL — nem o ack de uma operacao descartada nem o prazo das legendas valem.
#   cenarios: (padrao) renovacao de CDN no resume interrompendo seek do
#   usuario; "ack" acks atrasados de alvos antigos; "sem-ack" prazo de 10 s
#   das legendas sem EV_BUSCOU; "ack-tardio" ack valido do alvo atual que
#   chega depois do prazo, dentro dos 15 s do host; "zero" seek enfileirado
#   a 0 s na espera.
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
  ${CC:-cc} -O1 -g -Wall -DNV_TPK $flag -DNV_TEST_DIRECT_CDN -Isrc ${flags[@]+"${flags[@]}"} tests/rede_tpk728_lifecycle.c src/video_tpk.c src/mkvfix.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c \
    src/farolerro.c src/farolanel.c src/fontesonda.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle"
  "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle"          # renovacao do CDN no resume
  "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle" ack      # acks atrasados de alvos antigos
  "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle" sem-ack  # prazo das legendas sem ack
  "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle" ack-tardio  # ack do alvo atual apos o prazo
  "${TMPDIR:-/tmp}/nuvio-tpk728-lifecycle" zero     # seek a 0 s enfileirado na espera
done
