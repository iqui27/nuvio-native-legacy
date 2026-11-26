#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #772/F83: a guarda de divergencia do renderer compara a leitura do relogio
# ANTES do mapa legsync com o numero cru do video (ver tests/ass_posicao772.c).
#   escala — AutoSync 25->23,976 fps real: fala 103-110 s visivel em 210/210
#            quadros (17a20ba8f desenhava 0/210 em ASS e SRT).
#   cruza  — mesmo mapa, fala terminando ao cruzar o limiar: 0 imagens no
#            intervalo vazio 36,80-37,00 s (17a20ba8f congelava 12/12).
#   bordas — sem mapa: saltos de 1,49/1,50/1,51 s, pausa, retomada, offset
#            de busca +2000 ms e troca de faixa seguem funcionando.
#   bash tests/ass_posicao772.sh    SANITIZE=1 (ASan/UBSan) ou SANITIZE=thread
set -u
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}
D=$(mktemp -d "$T/nv-pos772.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
if [ "$(uname -s)" = Darwin ]; then
  platform=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -framework OpenGL)
else
  read -ra platform <<< "$(pkg-config --cflags --libs sdl2 gl)"
fi
"${CC:-cc}" -DNV_ASS_LIBASS -DNV_TEST_PLAINASS_LINKED -include tests/ass_pisca_gl.h -Isrc -Itests \
  tests/ass_posicao772.c src/assrender.c src/assperf.c src/assfios.c src/plainass.c src/relogio.c \
  src/autosync.c src/legenda.c tests/ass_render_stub.c \
  -o "$D/test" -O1 -g -Wall -lm "${flags[@]}" \
  $(pkg-config --cflags --libs libass fribidi) "${platform[@]}" -pthread \
  -Wno-deprecated-declarations -Wno-macro-redefined || exit 1
# Fontes/trilhas do libass e do fontconfig nao sao liberadas em atexit; a
# corrida que sobra no TSan e interna do libharfbuzz nao instrumentado.
if [ "${SANITIZE:-0}" = thread ]; then
  printf 'race:libharfbuzz\n' > "$D/tsan.supp"
  export TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}suppressions=$D/tsan.supp"
fi
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
rc=0
for caso in bordas escala cruza; do
  for fmt in ass srt; do
    r=0
    "$D/test" "$caso" "$fmt" || r=$?
    printf 'RESULT %s %s sanit=%s rc=%s\n' "$caso" "$fmt" "${SANITIZE:-0}" "$r"
    [ "$r" = 0 ] || rc=1
  done
done
[ "$rc" = 0 ] && echo "PASS: divergencia medida na leitura antes do mapa (#772)"
exit "$rc"
