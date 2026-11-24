#!/bin/bash
# Teste integrado da Tela 1 (coluna Achar o que ver) e Tela 7 ("Achados") — issue #25.
# Cobre logica + D-pad + cat_revisao() + honestidade + resolver_pediu_abrir e
# captura PNG da Tela 1 (4 intencoes) e Tela 7 (3 cartazes) em diretorio temporario.
#   bash tests/farol_acha_tela.sh
#   SANITIZE=0 bash tests/farol_acha_tela.sh
set -eu
cd "$(dirname "$0")/.."

# Contratos de portabilidade e higiene (#25).
if grep -n "_Thread_local" src/farolachaui.c src/farolachaui.h src/resolver.c src/resolver.h; then
  echo "farol_acha_tela: _Thread_local proibido" >&2; exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log|LOGD|puts|fputs)[[:space:]]*\(' src/farolachaui.c; then
  echo "farol_acha_tela: log proibido em src/farolachaui.c" >&2; exit 1
fi
if git diff --quiet HEAD -- src/app.c 2>/dev/null; then :; else
  if git diff HEAD -- src/app.c | grep -nE '^[+-].*(urlCanalRota|retentarCanal)'; then
    echo "farol_acha_tela: src/app.c mexeu no canal ao vivo" >&2; exit 1
  fi
fi

flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acha-tela.XXXXXX")
trap 'rm -rf "$work"' EXIT

python3 tests/farol_resolver_ponte.py "$work"

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -Wall -Wextra -Wno-unused-function -O1 -g \
  tests/farol_acha_tela.c src/resolver.c src/farolachaui.c src/farolacha.c \
  src/resolverregra.c src/farolregra.c src/farol.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c src/farolacao_addon.c src/addonstats.c \
  -I"$work" -Isrc -Itests $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lm -lpthread \
  -Wl,--wrap=SDL_GetTicks \
  -o "$work/farol-acha-tela-test"

if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-acha-tela-test"
else
  "$work/farol-acha-tela-test"
fi

# Captura PNG da Tela 1 (4 intencoes) e Tela 7 (3 cartazes) em diretorio temporario.
shot_fl=(-O1 -g -Isrc -DAJUSTES_TESTE -DAVISOS_TESTE_ENVIO -DRESOLVER_TESTE
         -DNV_ACHA_TELA_SHOT -Wall -Wextra -Wno-deprecated-declarations)
case "$("$ccbin" --version 2>/dev/null | head -n1)" in
  *clang*|*Clang*) shot_fl+=(-Wno-macro-redefined) ;;
esac
if [ "${SANITIZE:-1}" = 1 ]; then
  shot_fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

libs=()
pode_shot=1
if [ "$(uname -s)" = "Darwin" ]; then
  shot_fl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
elif pkg-config --exists sdl2 SDL2_image SDL2_ttf && pkg-config --exists glesv2; then
  read -r -a cf <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2)"
  read -r -a lf <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2)"
  shot_fl+=("${cf[@]}" -DNV_LINUX_DESKTOP)
  libs=("${lf[@]}" -lz -pthread -lm -ldl)
else
  pode_shot=0
fi

runner=()
if [ "$pode_shot" = 1 ]; then
  if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
  if [ "$(uname -s)" != "Darwin" ] && [ -z "${DISPLAY:-}" ]; then
    if command -v xvfb-run >/dev/null 2>&1; then
      runner=(xvfb-run -a -s "-screen 0 1920x1080x24")
    else
      pode_shot=0
    fi
  fi
fi

if [ "$pode_shot" = 1 ]; then
  NUVIO_DADOS="$work/dados"
  mkdir -p "$NUVIO_DADOS" "$work/shot" "$work/obj"
  export NUVIO_DADOS
  ls src/*.c src/dts/*.c | grep -v '/main\.c$' > "$work/fontes"
  export ccbin objdir="$work/obj"
  printf '%s\0' "${shot_fl[@]}" > "$work/flags"
  jobs="${FAROL_SHOT_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
  # shellcheck disable=SC2016
  xargs -a "$work/fontes" -P "$jobs" -I{} bash -c '
    mapfile -d "" f < "$0"
    o="$objdir/$(echo "{}" | tr / _).o"
    "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags"
  "$ccbin" "${shot_fl[@]}" tests/farol_acha_tela.c "$work/obj"/*.o \
    -o "$work/farol-acha-shot" "${libs[@]}"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    "${runner[@]+"${runner[@]}"}" "$work/farol-acha-shot" "$work/shot/cap"

  for f in "$work/shot/cap-tela1.png" "$work/shot/cap-tela7.png"; do
    [ -s "$f" ] || { echo "farol_acha_tela: captura ausente ou vazia: $f" >&2; exit 1; }
    python3 -c 'import sys; d=open(sys.argv[1],"rb").read(8); assert d==b"\x89PNG\r\n\x1a\n"' "$f"
  done
  echo "farol_acha_tela: capturas PNG validadas e removidas na saida"
else
  echo "farol_acha_tela: AVISO - sem display/GLES2 para captura PNG; logica validada" >&2
fi

echo "farol_acha_tela: tudo ok"
