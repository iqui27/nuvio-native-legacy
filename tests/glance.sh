#!/bin/bash
# PREVIA RAPIDA (glance, 2.1): o OK curto num cartaz abre a previa em vez da
# pagina do titulo; segurar OK continua abrindo o menu do cartaz.
#
# Duas partes: (1) o contrato nos fontes — cada tela que lista titulos abre a
# previa no toque, o roteador do app a encaminha e o hold nao mudou; (2) o
# modelo de navegacao de src/glance.c rodando de verdade (tests/glance.c).
#
#   bash tests/glance.sh
set -euo pipefail
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-glance-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
falhou=0
falha() { echo "  FALHOU: $1"; falhou=1; }

[ -f src/glance.h ] && [ -f src/glance.c ] || falha "src/glance.c/.h nao existem"

# Salvar / Marcar como visto reusam o pipeline do menu do cartaz (Trakt, Simkl,
# lista local, espelho), sem reescrever a rede: ctxmenu.c ganha a acao rapida.
grep -q 'ctx_acao_rapida' src/ctxmenu.h || falha "ctxmenu.h sem ctx_acao_rapida"

# Toque curto = previa, nas telas com lista. O hold (CTXH_LONGO) segue o menu.
for f in vertudo busca biblioteca home detail; do
  grep -q 'glance_abrir' "src/$f.c" || falha "$f.c nao abre a previa no toque"
done
grep -q 'case CTXH_LONGO: if (!menuNoFocado()) abrirFocado(); return;' src/vertudo.c \
  || falha "vertudo.c: o hold (CTXH_LONGO) mudou"

# O roteador do app: evento, quadro e desenho; e a previa vem ANTES do menu do
# cartaz no evento (ela pode abrir por cima do menu? nao: o menu so abre sem ela).
for fn in glance_evento glance_atualizar glance_desenhar; do
  grep -q "$fn" src/app.c || falha "app.c nao chama $fn"
done

# A previa fecha o trailer da home enquanto aberta (nao dois videos).
grep -q 'glance_aberto' src/home.c || falha "home.c: o trailer do destaque ignora a previa"

if [ "$falhou" != 0 ]; then echo "glance: contrato FALHOU"; exit 1; fi

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
make -s -f tools/native-objects.mk -j 6 CC=cc OBJDIR="$tmp/obj" \
  SOURCES="${sources[*]} tests/glance.c" \
  NATIVE_FLAGS='-O0 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations' > "$tmp/build.log" 2>&1 \
  || { tail -60 "$tmp/build.log"; exit 1; }
objects=()
for source in "${sources[@]}" tests/glance.c; do objects+=("$tmp/obj/${source%.c}.o"); done
cc "${objects[@]}" -o "$tmp/teste" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_DADOS="$tmp/dados" "$tmp/teste" "$@" || falhou=1
if [ "$falhou" != 0 ]; then echo "glance.sh: FALHOU"; exit 1; fi
echo "glance.sh: ok"
