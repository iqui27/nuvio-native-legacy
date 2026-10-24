#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas das fileiras de TEMPORADA e de EPISODIO da pagina de titulo, em PNG,
# sem rede e sem interacao. Nao entra na suite (testa-tudo.sh pula *_shot.sh):
# precisa de janela GL e de olho humano para julgar.
#
#   bash tests/detail_eps_shot.sh /tmp/nuvio-deteps
#
# Compila tudo MENOS src/main.c e src/detail.c, que a captura inclui — ela
# precisa do foco, do nivel e da rolagem, que sao estaticos de detail.c, e
# intercepta extras.h por #define para nao depender de rede nenhuma.
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA: modulos da pagina gravam cache,
# e sem isso escreveriam no ~/.nuvio de quem executa.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-deteps-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  case "$source" in
    src/reacao.c|src/notasui.c)
      if [ -n "${NV_REACAO_BASELINE:-}" ]; then source="$NV_REACAO_BASELINE/${source#src/}"; fi;;
  esac
  sources+=("$source")
done

baseline_flags=()
if [ -n "${NV_REACAO_BASELINE:-}" ]; then
  baseline_flags+=("-DNV_REACAO_DETAIL_SOURCE=\"$NV_REACAO_BASELINE/detail.c\"")
fi
cc "${sources[@]}" tests/detail_eps_shot.c "${baseline_flags[@]}" -Isrc -o ${TMPDIR:-/tmp}/nuvio-deteps-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined ${NV_CC_EXTRA:-}
${TMPDIR:-/tmp}/nuvio-deteps-shot "$@"
