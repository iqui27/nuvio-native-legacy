#!/bin/bash
# Tela P4 "Comparar" (lote sense, issue 49).
#
#   SANITIZE=1 bash tests/farol_compara.sh   (ASan+UBSan, padrao)
#   SANITIZE=0 bash tests/farol_compara.sh   (normal)
#
# Liga contra o app inteiro (menos main.c). As capturas PNG de 3 e 2 colunas
# vao para o diretorio do run e saem no fim (FAROL_SHOT_DIR guarda copia).
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_compara: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
case "$san" in
  0|1) ;;
  *) echo "farol_compara: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol-compara.XXXXXX")
trap 'rm -rf "$work"' EXIT

# 1. Contratos estaticos: sem log (privacidade), sem _Thread_local.
if grep -nE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD|nv_log)[[:space:]]*\(' \
     src/farolcompara.c src/farolcompara.h; then
  echo "farol_compara: FALHA (log em src/farolcompara.c/.h)" >&2
  exit 1
fi
if grep -n '_Thread_local' src/farolcompara.c src/farolcompara.h tests/farol_compara.c; then
  echo "farol_compara: FALHA (_Thread_local)" >&2
  exit 1
fi

# O roteador revalida o indice guardado pelo id: com o catalogo deslocado,
# cmpK>=0 sem a identidade abre (e toca) outro titulo.
if ! grep -q 'cmpK=cat_indice_vivo(cmpK,cmpRef)' src/app.c; then
  echo "farol_compara: FALHA (app.c nao revalida cmpK pela identidade cmpRef)" >&2
  exit 1
fi
if grep -q 'cmpK=cat_indice_titulo(cmpRef,-1)' src/app.c; then
  echo "farol_compara: FALHA (app.c ignora cmpRef quando cmpK >= 0)" >&2
  exit 1
fi

if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2; then
  echo "farol_compara: SDL2/SDL2_image/SDL2_ttf/GLESv2 indisponivel" >&2
  exit 1
fi
read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_LINUX_DESKTOP -Wall -Wextra -Wno-deprecated-declarations)
if [ "$san" = "1" ]; then
  fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

objdir="$TMPDIR/nuvio-farol49-obj-s${san}"
mkdir -p "$objdir"
objs=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c) continue ;; esac
  o="$objdir/$(basename "$s" .c).o"
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
    "$ccbin" "${fl[@]}" -c "$s" -o "$o"
  fi
done
# O modulo novo compila sem aviso nenhum.
"$ccbin" "${fl[@]}" -Werror -c src/farolcompara.c -o "$work/farolcompara.o"

"$ccbin" "${fl[@]}" -Werror tests/farol_compara.c "${objs[@]}" -o "$work/t" \
  "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

dados="$work/dados"
mkdir -p "$dados"
export NUVIO_DADOS="$dados"
shots="$work/shots"
mkdir -p "$shots"
# O driver "dummy" do SDL nao abre GL: a janela escondida precisa do X
# (padrao de tests/farol_shot.sh — o runner exporta dummy quando nao ha X).
if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a "$work/t" "$shots"
else
  "$work/t" "$shots"
fi

for f in comparar-3 comparar-2; do
  if [ ! -s "$shots/$f.png" ] || ! head -c 8 "$shots/$f.png" | od -An -tx1 | grep -q '89 50 4e 47'; then
    echo "farol_compara: FALHA (captura $f.png invalida)" >&2
    exit 1
  fi
done
if [ -n "${FAROL_SHOT_DIR:-}" ]; then
  mkdir -p "$FAROL_SHOT_DIR"
  cp "$shots"/*.png "$FAROL_SHOT_DIR"/
fi

echo "farol_compara: tudo ok (SANITIZE=$san)"
