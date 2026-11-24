#!/bin/bash
# Assistente de escolha P5 sem LLM (lote sense, issue 50).
#
#   SANITIZE=1 bash tests/farol_escolha.sh   (ASan+UBSan, padrao)
#   SANITIZE=0 bash tests/farol_escolha.sh   (normal)
#   CC=clang SANITIZE=1 bash tests/farol_escolha.sh
#
# 1) contratos estaticos (motor puro, sem log, sem _Thread_local);
# 2) motor puro sozinho (-DFESC_SO_MOTOR, sem SDL): 1.000 perfis sinteticos;
# 3) o app inteiro (menos main.c): Farol real, D-pad, P4, 1 e 0 resultados e
#    as capturas PNG 1920x1080, que saem no fim (FAROL_SHOT_DIR guarda copia).
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_escolha: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
case "$san" in
  0|1) ;;
  *) echo "farol_escolha: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol-escolha.XXXXXX")
trap 'rm -rf "$work"' EXIT

# 1. Contratos estaticos.
motor=(src/farolescolha.c src/farolescolha.h)
if grep -nE '#[[:space:]]*include[[:space:]]*[<"](SDL|dados\.h|rede|net|curl|sys/socket|pthread)' "${motor[@]}"; then
  echo "farol_escolha: FALHA (motor nao e puro: SDL/dados/rede)" >&2
  exit 1
fi
if grep -nE '\b(malloc|calloc|realloc|free|SDL_[A-Za-z]+)[[:space:]]*\(' "${motor[@]}"; then
  echo "farol_escolha: FALHA (motor aloca ou chama SDL)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD|nv_log|SDL_Log)[[:space:]]*\(' \
     src/farolescolha.c src/farolescolha.h src/farolescolhaui.c src/farolescolhaui.h; then
  echo "farol_escolha: FALHA (log em src/farolescolha*)" >&2
  exit 1
fi
if grep -n '_Thread_local' src/farolescolha*.c src/farolescolha*.h tests/farol_escolha.c; then
  echo "farol_escolha: FALHA (_Thread_local)" >&2
  exit 1
fi
if [ "$(grep -c 'FT_ESC_' src/farolescolha.c)" -lt 9 ]; then
  echo "farol_escolha: FALHA (banco sem os rotulos FT_ESC_*)" >&2
  exit 1
fi

san_fl=()
if [ "$san" = "1" ]; then
  san_fl=(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
fi

# 2. Motor puro: so -Isrc, sem SDL nenhum no caminho.
"$ccbin" -O1 -g -Isrc -DFESC_SO_MOTOR -Wall -Wextra -Werror "${san_fl[@]}" \
  tests/farol_escolha.c src/farolescolha.c -o "$work/motor"
"$work/motor"

# 3. App inteiro.
if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2; then
  echo "farol_escolha: SDL2/SDL2_image/SDL2_ttf/GLESv2 indisponivel" >&2
  exit 1
fi
read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_LINUX_DESKTOP -Wall -Wextra -Wno-deprecated-declarations "${san_fl[@]}")

objdir="$TMPDIR/nuvio-farol50-obj-s${san}-$(basename "$ccbin")"
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
# Os modulos novos e o resolver compilam sem aviso nenhum.
for s in src/farolescolha.c src/farolescolhaui.c src/resolver.c; do
  "$ccbin" "${fl[@]}" -Werror -c "$s" -o "$work/$(basename "$s" .c).o"
done

"$ccbin" "${fl[@]}" -Werror tests/farol_escolha.c "${objs[@]}" -o "$work/t" \
  "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

dados="$work/dados"
mkdir -p "$dados"
export NUVIO_DADOS="$dados"
shots="$work/shots"
mkdir -p "$shots"
# O driver "dummy" do SDL nao abre GL: a janela escondida precisa do X.
if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a "$work/t" "$shots"
else
  "$work/t" "$shots"
fi

for f in escolha-inicio escolha-pergunta escolha-nada; do
  if [ ! -s "$shots/$f.png" ] || ! head -c 8 "$shots/$f.png" | od -An -tx1 | grep -q '89 50 4e 47'; then
    echo "farol_escolha: FALHA (captura $f.png invalida)" >&2
    exit 1
  fi
done
if [ -n "${FAROL_SHOT_DIR:-}" ]; then
  mkdir -p "$FAROL_SHOT_DIR"
  cp "$shots"/*.png "$FAROL_SHOT_DIR"/
fi

echo "farol_escolha: tudo ok (SANITIZE=$san CC=$ccbin)"
