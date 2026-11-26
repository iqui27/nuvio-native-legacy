#!/bin/bash
# Teste da issue #51 ("Mais assim", "Nao e para mim", "Comparar com...",
# 1 linha P6 no ctxinfo, Ajustes > Sobre e ajuda > Gosto, farolgostoui).
#
#   SANITIZE=0 bash tests/farol_gosto_acoes.sh
#   SANITIZE=1 bash tests/farol_gosto_acoes.sh
#   CC=clang SANITIZE=1 bash tests/farol_gosto_acoes.sh
set -euo pipefail
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  TMPDIR=/tmp
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
case "$san" in
  0|1) ;;
  *) echo "farol_gosto_acoes: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol51.XXXXXX")
trap 'rm -rf "$work"' EXIT

# 1. Contratos estaticos: sem _Thread_local, sem %s de titulo/perfil/gosto em logs
if grep -n '_Thread_local' src/farolgostoui.c src/farolgostoui.h \
     src/ctxmenu.c src/ctxinfo.c tests/farol_gosto_acoes.c; then
  echo "farol_gosto_acoes: FALHA (_Thread_local encontrado)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD|nv_log)[[:space:]]*\(' \
     src/farolgostoui.c src/farolgostoui.h; then
  echo "farol_gosto_acoes: FALHA (log encontrado em src/farolgostoui.c/.h)" >&2
  exit 1
fi
if ! git diff --check; then
  echo "farol_gosto_acoes: FALHA (git diff --check)" >&2
  exit 1
fi

if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2; then
  echo "farol_gosto_acoes: SDL2/SDL2_image/SDL2_ttf/GLESv2 indisponivel" >&2
  exit 1
fi
read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_LINUX_DESKTOP -DAJUSTES_TESTE \
    -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "$san" = "1" ]; then
  fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
fi

cc_tag="${ccbin##*/}"
objdir="$TMPDIR/nuvio-farol51-obj-${cc_tag}-s${san}"
mkdir -p "$objdir"
objs=()
to_build=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c|src/ajustes.c) continue ;; esac
  o="$objdir/$(basename "$s" .c).o"
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || \
     [ -n "$(find src -type f \( -name '*.h' -o -name '*.inc' \) -newer "$o" -print -quit)" ]; then
    to_build+=("$s")
  fi
done
if [ "${#to_build[@]}" -gt 0 ]; then
  export ccbin objdir
  export FL_STR="${fl[*]}"
  printf '%s\0' "${to_build[@]}" | xargs -0 -n 8 -P "$(nproc 2>/dev/null || echo 4)" \
    sh -c 'for s; do "$ccbin" $FL_STR -c "$s" -o "$objdir/$(basename "$s" .c).o" || exit 1; done' sh
fi

"$ccbin" "${fl[@]}" -Werror -c src/farolgostoui.c -o "$work/farolgostoui.o"
"$ccbin" "${fl[@]}" -c src/ctxinfo.c -o "$work/ctxinfo.o"
"$ccbin" "${fl[@]}" -c src/ctxmenu.c -o "$work/ctxmenu.o"

"$ccbin" "${fl[@]}" -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  tests/farol_gosto_acoes.c "${objs[@]}" -o "$work/t" \
  "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

dados="$work/dados"
mkdir -p "$dados"
export NUVIO_DADOS="$dados"

if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a "$work/t" "$work"
else
  "$work/t" "$work"
fi

echo "farol_gosto_acoes: tudo ok (CC=$cc_tag SANITIZE=$san)"
