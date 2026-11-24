#!/bin/bash
# Farol #16: acoes de audio e memoria (CONVERTER_AUDIO com Desfazer, TROCAR_FAIXA,
# ALIVIAR_REABRIR). Ver tests/farol_acao_audio.c.
#
#   bash tests/farol_acao_audio.sh              # ASan + UBSan (padrao)
#   SANITIZE=0 bash tests/farol_acao_audio.sh   # normal
set -eu
cd "$(dirname "$0")/.."

if grep -n "_Thread_local" src/farolacao_audio.c src/farolacao_audio.h; then
  echo "farol_acao_audio: _Thread_local proibido" >&2; exit 1
fi
if grep -n "NV_DTS_FFMPEG" src/*.h; then
  echo "farol_acao_audio: NV_DTS_FFMPEG nao pode aparecer em headers" >&2; exit 1
fi
if grep -n "SDL2/SDL" src/farolacao_audio.h; then
  echo "farol_acao_audio: a decisao pura nao pode puxar SDL" >&2; exit 1
fi
# O executor so conversa pelas ops: nada de rede e nada do canal ao vivo.
if grep -nE "urlCanalRota|retentarCanal|watchdog" src/farolacao_audio.c src/farolacao_audio.h; then
  echo "farol_acao_audio: o modulo nao toca o canal ao vivo" >&2; exit 1
fi

flags=(-Wall -Wextra -O1 -g)
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acao-audio.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$ccbin" "${flags[@]}" -I/opt/homebrew/include \
  tests/farol_acao_audio.c src/farolacao_audio.c src/memlog.c src/farolanel.c \
  src/farolregra.c src/resolverregra.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -lpthread -o "$work/farol-acao-audio"
"$work/farol-acao-audio"

echo "farol_acao_audio: tudo ok"
