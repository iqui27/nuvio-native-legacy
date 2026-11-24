#!/bin/bash
# Capturas da tela do Farol (issue 19) em PNG 1920x1080, no padrao de
# tests/resolver_shot.sh: o app inteiro (src/*.c) linkado com RESOLVER_TESTE,
# janela GL escondida e FBO. Linux usa GLES2 (Mesa) sob xvfb-run quando nao ha
# DISPLAY; macOS usa o OpenGL do sistema. Fica fora da suite (*_shot.sh).
#
#   bash tests/farol_shot.sh tests/out/farol
#   SANITIZE=1 bash tests/farol_shot.sh tests/out/farol   (ASan+UBSan)
#   FAROL_SHOT_FONTE=tests/resolver_shot.c bash tests/farol_shot.sh tests/out/resolver
#     (o mesmo build para as capturas antigas; e o caminho Linux de
#     tests/resolver_shot.sh)
#
# FALLBACK: sem SDL2/SDL2_image/SDL2_ttf + GLES2/GL ou sem como abrir um
# contexto GL (sem DISPLAY e sem xvfb-run), sai com 0 e avisa que as capturas
# NAO foram feitas — a logica da tela segue coberta por tests/farol_tela.sh.
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nuvio-farol}"
mkdir -p "$(dirname "$saida")"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
export NUVIO_DADOS

ccbin="${CC:-cc}"
fl=(-O1 -g -Isrc -DAJUSTES_TESTE -DAVISOS_TESTE_ENVIO -DRESOLVER_TESTE
    -Wall -Wextra -Wno-deprecated-declarations)
case "$("$ccbin" --version 2>/dev/null | head -n1)" in
  *clang*|*Clang*) fl+=(-Wno-macro-redefined) ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then
  fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
libs=()
if [ "$(uname -s)" = "Darwin" ]; then
  fl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
elif pkg-config --exists sdl2 SDL2_image SDL2_ttf && pkg-config --exists glesv2; then
  read -r -a cf <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2)"
  read -r -a lf <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2)"
  fl+=("${cf[@]}" -DNV_LINUX_DESKTOP)
  libs=("${lf[@]}" -lz -pthread -lm -ldl)
else
  echo "farol_shot: AVISO - SDL2/SDL2_image/SDL2_ttf/GLES2 ausentes; capturas NAO feitas (logica em tests/farol_tela.sh)" >&2
  exit 0
fi
# O driver "dummy" do SDL nao abre GL: a janela escondida precisa do X (ou do Mac).
if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
runner=()
if [ "$(uname -s)" != "Darwin" ] && [ -z "${DISPLAY:-}" ]; then
  if command -v xvfb-run >/dev/null 2>&1; then
    runner=(xvfb-run -a -s "-screen 0 1920x1080x24")
  else
    echo "farol_shot: AVISO - sem DISPLAY nem xvfb-run; capturas NAO feitas (logica em tests/farol_tela.sh)" >&2
    exit 0
  fi
fi

# Objetos em paralelo (o app inteiro passa de 470 arquivos).
objdir="$work/obj"
mkdir -p "$objdir"
ls src/*.c src/dts/*.c | grep -v '/main\.c$' > "$work/fontes"
export ccbin objdir
printf '%s\0' "${fl[@]}" > "$work/flags"
jobs="${FAROL_SHOT_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
# shellcheck disable=SC2016
xargs -a "$work/fontes" -P "$jobs" -I{} bash -c '
  mapfile -d "" f < "$0"
  o="$objdir/$(echo "{}" | tr / _).o"
  "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags"
fonte="${FAROL_SHOT_FONTE:-tests/farol_shot.c}"
"$ccbin" "${fl[@]}" "$fonte" "$objdir"/*.o -o "$work/farol-shot" "${libs[@]}"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
  "${runner[@]+"${runner[@]}"}" "$work/farol-shot" "$saida"
echo "farol_shot: tudo ok"
