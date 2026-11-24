#!/bin/bash
# FAROL — a tela (issue 19): Inicio, Cadeia e Veredito de src/resolver.c por
# sequencias reais de D-pad/OK/Voltar, sobre a fachada src/farol.c e os trechos
# de producao de src/player.c e src/app.c (os mesmos de tests/farol_resolver.sh).
# Ver o cabecalho de tests/farol_tela.c.
#
#   bash tests/farol_tela.sh                (ASan+UBSan, padrao)
#   SANITIZE=0 bash tests/farol_tela.sh     (normal)
set -eu
cd "$(dirname "$0")/.."

# Contratos de portabilidade da tela (Tizen TPK40 e headers sem FFmpeg).
if grep -n "_Thread_local" src/resolver.c src/resolver.h src/resolverregra.c src/resolverregra.h; then
  echo "farol_tela: _Thread_local proibido na tela do Farol" >&2; exit 1
fi
if grep -n "NV_DTS_FFMPEG" src/*.h; then
  echo "farol_tela: NV_DTS_FFMPEG nao pode aparecer em headers" >&2; exit 1
fi
# A tela fala pelo i18n: o nome e as entradas do Farol saem de faroltexto.h.
for k in FT_NOME FT_AJUDA_KICKER FT_BANNER_FALHA FT_SINTOMA_CORES FT_ACHAR_O_QUE_VER \
         FT_REGISTROS_ENVIADOS FT_TESTANDO FT_NAO_TESTADO FT_A_CAUSA_ENCONTRADA; do
  if ! grep -q "$k" src/resolver.c; then
    echo "farol_tela: src/resolver.c nao usa $k" >&2; exit 1
  fi
done
# app.c so despacha o que resolver_* pede: nada de canal ao vivo aqui.
if git diff --quiet HEAD -- src/app.c 2>/dev/null; then :; else
  if git diff HEAD -- src/app.c | grep -nE '^[+-].*(urlCanalRota|retentarCanal)'; then
    echo "farol_tela: src/app.c mexeu no canal ao vivo" >&2; exit 1
  fi
fi

flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-tela.XXXXXX")
trap 'rm -rf "$work"' EXIT

python3 tests/farol_resolver_ponte.py "$work"

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -Wall -Wextra -Wno-unused-function -O1 -g \
  tests/farol_tela.c src/resolver.c src/farolachaui.c src/farolacha.c \
  src/resolverregra.c src/farolregra.c src/farol.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c \
  -I"$work" -Isrc -Itests $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lm -lpthread \
  -o "$work/farol-tela-test"

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -Wall -Wextra -Wno-unused-function -O1 -g \
  tests/farol_tela_sonda.c src/resolver.c src/farolachaui.c src/farolacha.c \
  src/resolverregra.c src/farolregra.c src/farol.c \
  src/farolerro.c src/farolanel.c src/farolsonda.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c \
  -I"$work" -Isrc -Itests $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lm -lpthread \
  -o "$work/farol-tela-sonda-test"

if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-tela-test"
  setarch "$(uname -m)" -R "$work/farol-tela-sonda-test"
else
  "$work/farol-tela-test"
  "$work/farol-tela-sonda-test"
fi
echo "farol_tela: tudo ok"
