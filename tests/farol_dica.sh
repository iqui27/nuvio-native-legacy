#!/bin/bash
# Cartao de dica do Farol (P2 — lote sense, issue 48).
# Parte 1: nucleo puro (3 dias com relogio injetado, cota 1/dia/perfil,
#          avisodisp persistido, todos os 6 acao.tipo, portoes, concorrencia).
# Parte 2: integracao real com src/home.c (coluna 0 em farol_gosto1, foco,
#          acao: nada fora da ordem de foco, OK nas acoes, Voltar longo,
#          movimento reduzido).
#
#   TMPDIR=... bash tests/farol_dica.sh                    (normal)
#   TMPDIR=... SANITIZE=1 bash tests/farol_dica.sh         (ASan+UBSan)
#   TMPDIR=... CC=clang SANITIZE=thread bash tests/farol_dica.sh  (TSan)
set -eu
if [ -z "${TMPDIR:-}" ]; then
  echo "farol_dica: defina TMPDIR fora de /tmp" >&2
  exit 1
fi
TMPDIR="${TMPDIR%/}"
cd "$(dirname "$0")/.."

CC_BIN="${CC:-cc}"
SAN="${SANITIZE:-0}"
flags=()
if [ "$SAN" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
elif [ "$SAN" = thread ]; then
  flags+=(-fsanitize=thread -fno-omit-frame-pointer)
fi

bin1="$TMPDIR/nuvio-farol-dica-core-$$"
bin2="$TMPDIR/nuvio-farol-dica-home-$$"
trap 'rm -f "$bin1" "$bin2"' EXIT

# Parte 1: nucleo + concorrencia (roda em normal, ASan+UBSan e TSan)
"$CC_BIN" ${flags[@]+"${flags[@]}"} -DNV_FAROLDICA_PURO -DNV_MAPA_PURO \
  tests/farol_dica.c \
  src/faroldica.c src/farolcena.c src/farolpeca.c src/faroltexto.c \
  src/farolrec.c src/farolgosto.c src/farolficha.c src/farolinfo.c \
  src/farolacha.c src/avisodisp.c src/catalogo.c src/progresso.c \
  src/salvos.c src/diario.c src/diarionomes.c src/dados.c src/js.c src/mapa.c \
  -Isrc -O1 -g -pthread -lm \
  -Wall -Wno-deprecated-declarations -Wno-unused-function \
  -o "$bin1"
"$bin1"

# Parte 2: integracao real com src/home.c (normal e ASan+UBSan; TSan pula
# porque home.c usa SDL/estado de fio unico — a concorrencia foi coberta na Parte 1).
if [ "$SAN" != thread ]; then
  if [ "$(uname -s)" = Darwin ]; then
    gc_flags=(-Wl,-dead_strip)
    sdl_inc=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  else
    gc_flags=(-Wl,--gc-sections)
    sdl_inc=(-I/usr/include/SDL2)
  fi
  "$CC_BIN" ${flags[@]+"${flags[@]}"} -DNV_MAPA_PURO -DCURADORIA_TESTE \
    -ffunction-sections -fdata-sections "${gc_flags[@]}" \
    tests/farol_dica_home.c tests/amigosfil_stub.c \
    src/faroldica.c src/farolcena.c src/farolpeca.c src/faroltexto.c \
    src/farolrec.c src/farolgosto.c src/farolficha.c src/farolinfo.c \
    src/farolacha.c src/avisodisp.c src/catalogo.c src/progresso.c \
    src/salvos.c src/diario.c src/diarionomes.c src/dados.c src/js.c src/mapa.c \
    src/focus.c src/posterprov.c src/redeurl.c \
    src/colecoes.c src/catordem.c src/fileiras.c src/homefil.c \
    src/artehero.c src/cwordem.c src/cwretido.c src/curadoria.c \
    src/buscanorm.c \
    -Isrc "${sdl_inc[@]}" -O1 -g -pthread -lm \
    -Wall -Wno-deprecated-declarations -Wno-unused-function \
    -o "$bin2"
  "$bin2"
fi

echo "farol_dica: tudo ok"
