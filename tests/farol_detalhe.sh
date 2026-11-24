#!/bin/bash
# Bloco "Por que voce vai gostar" (P3) + informacao relevante (P6) no detalhe
# (lote sense, issue 46).
#
# Modos:
#   SANITIZE=1 bash tests/farol_detalhe.sh        (ASan+UBSan, padrao)
#   SANITIZE=0 bash tests/farol_detalhe.sh        (normal)
#   CC=clang SANITIZE=thread bash tests/farol_detalhe.sh  (TSan com setarch -R)
#
# Opcional: FAROL_SHOT_DIR=/caminho grava PNGs sem-bloco / com-bloco / com-bloco-selo.
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_detalhe: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  echo "  ex.: TMPDIR=/mnt/block-volume/nuvio-runs/... bash tests/farol_detalhe.sh" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
flags=(-Wall -Wextra -O1 -g -Isrc)
runner=()

case "$san" in
  0) ;;
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread -fno-omit-frame-pointer -fPIE)
    if command -v setarch >/dev/null 2>&1; then
      runner=(setarch "$(uname -m)" -R)
    fi
    ;;
  *)
    echo "farol_detalhe: SANITIZE desconhecido: $san" >&2
    exit 2
    ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol-detalhe.XXXXXX")
trap 'rm -rf "$work"' EXIT

if [ "$(uname -s)" = "Darwin" ]; then
  sdl_cflags="-I/opt/homebrew/include -I/opt/homebrew/include/SDL2"
else
  sdl_cflags="$(pkg-config --cflags sdl2 2>/dev/null || true)"
fi
link_extra=(-lm -lpthread)
if [ "$san" = "thread" ]; then link_extra+=(-pie); fi

# 1. Nucleo deterministico + prazo 8 s + fade + foco + 29 idiomas + TSan
"$ccbin" "${flags[@]}" $sdl_cflags \
  tests/farol_detalhe.c \
  src/faroldetalhe.c src/farolcena.c src/farolrec.c src/farolgosto.c \
  src/farolinfo.c src/farolpeca.c src/faroltexto.c src/farolficha.c \
  src/js.c src/idioma.c src/linguas.c \
  "${link_extra[@]}" -o "$work/core"

"${runner[@]+"${runner[@]}"}" "$work/core"

# 2. Integracao real com src/detail.c + GLES2 1920x1080 (pixel a pixel + D-pad)
#    Pula no modo TSan puro porque o driver GLES/Mesa do host nao e instrumentado
#    com TSan (a concorrencia do nosso codigo ja rodou instrumentada em $work/core).
if [ "$san" != "thread" ] && [ "${FAROL_SKIP_GL:-0}" != "1" ]; then
  gl_ok=1
  gl_defs=()
  gl_inc=()
  gl_libs=()
  if [ "$(uname -s)" = "Darwin" ]; then
    gl_inc=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
    gl_libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
  elif pkg-config --exists sdl2 SDL2_image SDL2_ttf; then
    read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
    read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
    gl_inc=("${sdk_flags[@]}")
    gl_libs=("${sdk_libs[@]}")
    gl_defs=(-DNV_LINUX_DESKTOP)
    if pkg-config --exists glesv2; then
      gl_libs+=(-lz -lGLESv2 -pthread -lm -ldl)
    elif pkg-config --exists gl; then
      gl_libs+=(-lz -lGL -pthread -lm -ldl)
    else
      gl_ok=0
    fi
  else
    gl_ok=0
  fi
  if [ "$gl_ok" != 1 ]; then
    echo "farol_detalhe: AVISO - SDL2/SDL2_image/SDL2_ttf ou GLESv2/GL indisponivel;" >&2
    echo "farol_detalhe: parte GL (detail.c real 1920x1080) NAO verificada neste host" >&2
  else
    objdir="$TMPDIR/nuvio-farol46-obj-s${san}"
    mkdir -p "$objdir"
    dados="$work/dados"
    mkdir -p "$dados"
    export NUVIO_DADOS="$dados"

    gl_fl=(-O1 -g -Isrc "${gl_inc[@]}" "${gl_defs[@]}"
           -Wall -Wextra -Wno-deprecated-declarations)
    case "$("$ccbin" --version 2>/dev/null | head -n1)" in
      *clang*|*Clang*) gl_fl+=(-Wno-macro-redefined) ;;
    esac
    if [ "$san" = "1" ]; then
      gl_fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
    fi

    objs=()
    for s in src/*.c src/dts/*.c; do
      case "$s" in src/main.c|src/detail.c) continue ;; esac
      o="$objdir/$(basename "$s" .c).o"
      objs+=("$o")
      if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
        "$ccbin" "${gl_fl[@]}" -c "$s" -o "$o"
      fi
    done

    "$ccbin" "${gl_fl[@]}" -DFD_MODO_GL tests/farol_detalhe.c "${objs[@]}" \
      -o "$work/gl" "${gl_libs[@]}"

    shot_dir="${FAROL_SHOT_DIR:-$work/shots}"
    mkdir -p "$shot_dir"
    if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
      xvfb-run -a "$work/gl" "$shot_dir"
    else
      "$work/gl" "$shot_dir"
    fi
  fi
fi

# 3. Contratos estaticos do modulo: sem malloc/free, sem log (privacidade ALMA)
#    e sem literais fora de faroltexto.h.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/faroldetalhe.c; then
  echo "farol_detalhe: FALHA (alocacao dinamica em src/faroldetalhe.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD)[[:space:]]*\(' src/faroldetalhe.c src/faroldetalhe.h; then
  echo "farol_detalhe: FALHA (log em src/faroldetalhe.c/.h)" >&2
  exit 1
fi

echo "farol_detalhe: tudo ok (SANITIZE=$san)"
