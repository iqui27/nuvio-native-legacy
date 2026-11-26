#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #772: travamentos/artefatos/saltos de 1-2 s em legendas ASS (Tizen). Compila
# e chama o codigo de verdade — o antigo script so fazia grep. Ver
# tests/ass_salto772.c.
#   [A] src/relogio.c: saltos +2/+5/+50/-57 s a 1x/1,5x/2x, engasgo >= 0,25 s
#       sem nova posicao (r->saida nao volta) e dois Relogios independentes.
#   [B] src/assrender.c (com libass): o salto visto na posicao do VIDEO
#       (assrender_posicao_video) derruba o quadro/pedido do instante velho
#       mesmo com o relogio da legenda parado.
#   [C] sonda sol-f64r2 incorporada: 24 quadros com o relogio parado na
#       origem (1x/2x, ida e volta, ASS e SRT via plainass), zero fala
#       antiga; passo comum mantem imagens.
#   bash tests/ass_salto772.sh    SANITIZE=1 (ASan/UBSan) ou SANITIZE=thread
set -eu
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}
D=$(mktemp -d "$T/nv-ass772.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libass; then
  if [ "$(uname -s)" = Darwin ]; then
    platform=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -framework OpenGL)
  else
    read -ra platform <<< "$(pkg-config --cflags --libs sdl2 gl)"
  fi
  "${CC:-cc}" -DNV_ASS_LIBASS -DNV_TEST_PLAINASS_LINKED -include tests/ass_pisca_gl.h -Isrc \
    tests/ass_salto772.c src/relogio.c src/assrender.c src/assperf.c src/asspeso.c src/assfios.c src/plainass.c tests/ass_render_stub.c \
    -o "$D/test" -O1 -g -Wall -lm "${flags[@]}" \
    $(pkg-config --cflags --libs libass fribidi) "${platform[@]}" -pthread \
    -Wno-deprecated-declarations -Wno-macro-redefined
  # Fontes/trilhas do libass e do fontconfig nao sao liberadas em atexit.
  # No TSan: a corrida que sobra e interna do libharfbuzz do sistema (nao
  # instrumentado) na saida do processo, com o worker ainda vivo — nada do
  # codigo do Nuvio aparece nos dois lados.
  if [ "${SANITIZE:-0}" = thread ]; then
    printf 'race:libharfbuzz\n' > "$D/tsan.supp"
    export TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}suppressions=$D/tsan.supp"
  fi
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$D/test"
else
  echo "ass_salto772: libass ausente — parte do renderer pulada"
  "${CC:-cc}" -Wall -O1 tests/ass_salto772.c src/relogio.c -o "$D/test" -lm "${flags[@]}"
  "$D/test"
fi
echo "PASS: salto ASS na posicao do video + relogio monotonico (#772)"
