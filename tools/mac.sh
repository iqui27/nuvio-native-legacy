#!/bin/bash
# Compila e roda no Mac, com a mesma pasta de arte do pacote da TV.
#
# FICA LOGADO ENTRE EXECUCOES. A sessao mora em $NUVIO_DADOS, que aqui aponta
# para ~/.nuvio: logue UMA vez (escaneando o QR da tela) e todo arranque
# seguinte abre com a conta dentro, porque o refresh token se renova sozinho.
# CONFERIDO: 1o arranque "[sessao] logado como ..."; reiniciando sem escanear
# nada, "[sessao] sessao restaurada ..." e "[addons] N vindos da conta".
#
# Para testar como um usuario NOVO (a primeira execucao de quem instala),
# aponte a variavel para uma pasta vazia:
#     NUVIO_DADOS=/tmp/nuvio-novo bash tools/mac.sh
#
# "Sair da conta", em Ajustes, apaga ~/.nuvio inteiro (sessao, ajustes e
# progresso) — depois disso e preciso escanear de novo.
set -e
cd "$(dirname "$0")/.."
# Os -D do servidor saem do MESMO local.properties do app web (tools/env.sh).
# Sem eles o app compila, instala e abre — e o unico sintoma e a tela de login
# dizendo "Este pacote foi montado sem servidor". Nenhum desses valores entra em
# arquivo de codigo versionado.
ENV_D=$(tools/env.sh)
# Fora da pasta do pacote: o Mac nao deve gravar sessao dentro de deploy/.
export NUVIO_DADOS="${NUVIO_DADOS:-$HOME/.nuvio}"
eval cc src/*.c -o /tmp/nuvio-native-legacy-mac -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations
exec /tmp/nuvio-native-legacy-mac "$(pwd)/deploy/app/art" "$@"
