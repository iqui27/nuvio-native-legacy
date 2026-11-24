#!/bin/bash
# Issue #21 do Farol: cartao de erro do player com veredito e botao primario de acao.
# Verifica:
#   - foco inicial 0, D-pad Esquerda/Direita 0 <-> 1 <-> 2 sem beco, OK 0/1/2
#   - acoes de fonte (PROXIMA_FONTE, FONTE_SEM_DV, FONTE_SEM_DTS, FONTE_MENOR)
#     executam farolacao_fonte_executar(acao, 0), reiniciam sessao e limpam erroFonte se aceitas
#   - demais acoes (DEBRID, PLAYER_EXTERNO, TENTAR_DE_NOVO, ENVIAR_REGISTRO)
#   - 2 botoes quando veredito e NADA_ACHADO ou acao = ABRIR_FONTES
#   - ordem: ultimoFarol e FE_ERRO_FONTE gravados ANTES de farol_julgar
#   - prioridade 1 de Xtream "linha em uso" sobre o Farol
#   - preservacao de #409 no Android (-DNV_ANDROID)
set -euo pipefail
cd "$(dirname "$0")/.."

if grep -n "_Thread_local" src/player.c src/app.c; then
  echo "farol_cartao: _Thread_local proibido" >&2; exit 1
fi

flags=(-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter
       -Wno-deprecated-declarations -Wno-macro-redefined
       -DRESOLVER_TESTE -DNV_LINUX_DESKTOP -Isrc -pthread -ffunction-sections -fdata-sections)
if pkg-config --exists sdl2 2>/dev/null; then
  read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdk[@]}")
fi
linker=-Wl,--gc-sections
[ "$(uname -s)" = Darwin ] && linker=-Wl,-dead_strip
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-cartao.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/dados" "$work/dados_android"

srcs=(
  src/farol.c
  src/farolregra.c
  src/resolverregra.c
  src/farolerro.c
  src/farolanel.c
  src/fontesonda.c
  src/farolacao_fonte.c
  src/streams.c
  src/fontefalha.c
  src/fonteauto.c
  src/limpa.c
  src/dobra.c
  src/fontenota.c
  src/linguas.c
  src/fontefiltro.c
  src/addonmodo.c
  src/extplayer.c
  src/badges.c
)

"$ccbin" "${flags[@]}" tests/farol_cartao.c "${srcs[@]}" "$linker" -lm -lpthread -o "$work/cartao"
NUVIO_DADOS="$work/dados" "$work/cartao"

"$ccbin" "${flags[@]}" -DNV_ANDROID -DNV_TOUCH_UI tests/farol_cartao.c "${srcs[@]}" "$linker" -lm -lpthread -o "$work/cartao_android"
NUVIO_DADOS="$work/dados_android" "$work/cartao_android"

echo "farol_cartao: tudo ok"
