#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
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
# NUVIO_MAC_BIN: outro binario (a auditoria nao pisa no /tmp/nuvio-native-legacy-mac
# de quem estiver rodando o app). NUVIO_MAC_SO_COMPILAR=1: compila e sai.
BIN="${NUVIO_MAC_BIN:-${TMPDIR:-/tmp}/nuvio-native-legacy-mac}"
# Nucleo EBML/NAL do mkv.c em Rust, ligado por padrao (docs/plans/rust-mapa.md);
# NUVIO_RUST_MKVCORE=0 volta ao caminho C.
FLAGS="-O1 -g $ENV_D -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations"
RUST_A=""
if [ "${NUVIO_RUST_MKVCORE:-1}" = 1 ]; then FLAGS="$FLAGS -DNV_RUST_MKVCORE"; RUST_A=$(tools/rust-mkvcore.sh); fi
# Mesmo construtor de objetos da TV: so recompila o .c alterado e quem inclui o
# header alterado. Flags/compilador diferentes = pasta de objetos diferente.
KEY=$( { printf "%s\n" "$FLAGS"; cc --version; cat tools/mac.sh tools/native-objects.mk; } | shasum -a 256 | cut -d" " -f1)
O="build/mac-objects/$KEY"
SRC=(src/*.c src/dts/*.c)
# make passa a receita pelo sh: os -D do env.sh chegam ao cc como no antigo eval.
make -s -f tools/native-objects.mk -j "$(getconf _NPROCESSORS_ONLN)" CC=cc OBJDIR="$O" \
  SOURCES="${SRC[*]}" NATIVE_FLAGS="${FLAGS//\$/\$\$}"
OBJ=("${SRC[@]/%.c/.o}")
cc "${OBJ[@]/#/$O/}" -o "$BIN" ${RUST_A:+"$RUST_A"} \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
[ -n "$NUVIO_MAC_SO_COMPILAR" ] && exit 0
exec "$BIN" "$(pwd)/deploy/app/art" "$@"
