#!/bin/bash
# Regra da tela de escolha de perfil. NAO precisa de rede nem de SDL: o teste
# alimenta perfis.c com as respostas que o servidor manda, transcritas do
# contrato (PLANO-CONTA-SYNC, secao 1.5).
#
# Duas ETAPAS em dois processos: "o segundo arranque" e um estado que so o
# disco atravessa, e fabrica-lo dentro do primeiro processo provaria outra
# coisa.
#
#   bash tests/perfilsel.sh              regra (entra na suite)
#   bash tests/perfilsel.sh --capturas   BMPs da tela (precisa de janela GL)
set -eu
cd "$(dirname "$0")/.."

# AS CAPTURAS SAO OUTRO TESTE, e nao entram na suite: precisam de janela GL e de
# olho humano para julgar. Mesma regra de tests/episodios_shot.sh.
if [ "${1:-}" = --capturas ]; then
  fontes=()
  for f in src/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
  cc "${fontes[@]}" tests/perfilsel_visual.c -Isrc -o /tmp/nuvio-perfilsel-shot \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
  D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
  # NUVIO_DADOS, e NAO uma variavel inventada: e a unica que dados_iniciar()
  # respeita. O argumento de dados_iniciar e a pasta de ARTE, ultimo candidato
  # da fila — passar a pasta temporaria por ali nao desvia nada, e a escrita
  # cai em ~/.nuvio, os dados REAIS de quem roda o teste. Aconteceu.
  NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-perfilsel-shot
  exit 0
fi
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# So perfis.c, js.c e jsw.c: a regra nao depende de SDL, de rede nem da tela, e
# linkar o app inteiro aqui tornaria o teste lento e fragil.
cc "${flags[@]}" src/perfis.c src/js.c src/jsw.c tests/perfilsel.c \
  -Isrc -o /tmp/nuvio-perfilsel-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined

DIR=$(mktemp -d); trap 'rm -rf "$DIR"' EXIT
NUVIO_TESTE_DIR="$DIR" /tmp/nuvio-perfilsel-tests
NUVIO_TESTE_DIR="$DIR" /tmp/nuvio-perfilsel-tests dois
echo "perfilsel: tudo ok"
