#!/bin/bash
# #728 (r3): corridas do ack de seek e a borda de 2,5 s do pouso do
# progressivo no TPK (ver tests/tpk728_ack.c e tests/tpk728_dts_borda.c).
# Host e conversor falsos; relogio controlado; ordem forcada pelos pontos
# -DNV_TEST_TPK_ACK. Sem rede, sem GL, sem aparelho.
#   cenarios do ack: (padrao) distinto; "prazo" ack na janela do -2 das
#   legendas; "repetido" ack velho com o mesmo alvo; "supersede" e
#   "supersede-prazo" seek novo entrando depois da leitura do ack;
#   "relogio" seek a 0 s sobre a retomada de 12 s. r4 (#728): a geracao vai
#   ao host e volta no ack — "fila" e "falha" imitam a fila do Video.cs,
#   "mesmo-depois" e "prazo-depois" forcam a ordem depois da trava do ack.
#   Da borda: (padrao)
#   quadro a exatos 2,5 s do alvo; "fora" 2,6 s coberto e 2,4 s liberado.
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
  # NV_MKVFIX_SEM_REDE: a sonda do mkvfix roda em fio real e acabaria em tempo
  # nao deterministico, segurando mf.pend (busca_em_voo) fora do cenario.
  ${CC:-cc} -O1 -g -Wall -DNV_TPK $flag -DNV_TEST_DIRECT_CDN -DNV_TEST_TPK_ACK -DNV_MKVFIX_SEM_REDE -Isrc ${flags[@]+"${flags[@]}"} tests/tpk728_ack.c src/video_tpk.c src/mkvfix.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c \
    src/farolerro.c src/farolanel.c src/fontesonda.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk728-ack"
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack"                 || rc=1  # ack de alvo velho descartado
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" prazo           || rc=1  # ack na janela do -2 das legendas
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" repetido        || rc=1  # ack velho com o mesmo alvo
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" supersede       || rc=1  # seek novo apos a leitura do ack
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" supersede-prazo || rc=1  # idem, com -2 gravado de novo
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" relogio         || rc=1  # seek a 0 s sobre a retomada
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" fila            || rc=1  # fila do host: so o ack de D confirma
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" falha           || rc=1  # EV_BUSCA_FALHOU aposenta a operacao
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" mesmo-depois    || rc=1  # seek novo ao mesmo alvo apos a trava
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" prazo-depois    || rc=1  # -2 do prazo apos a trava do ack
  "${TMPDIR:-/tmp}/nuvio-tpk728-ack" falha-isolada   || rc=1  # falha corrente aposenta a reabertura pausada
  ${CC:-cc} -O1 -g -Wall -DNV_TPK $flag -Isrc ${flags[@]+"${flags[@]}"} tests/tpk728_dts_borda.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c \
    src/farolerro.c src/farolanel.c src/fontesonda.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk728-dts-borda"
  "${TMPDIR:-/tmp}/nuvio-tpk728-dts-borda"       || rc=1  # borda inclusiva: 2,5 s libera
  "${TMPDIR:-/tmp}/nuvio-tpk728-dts-borda" fora  || rc=1  # 2,6 s coberto, 2,4 s liberado
done
exit $rc
