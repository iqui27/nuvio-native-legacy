#!/bin/bash
# Quantas vezes o alvo Tizen baixa um GIF (ver tests/texgif_tizen.c). Node,
# pthreads do Emscripten de verdade, XMLHttpRequest falso que conta pedidos.
#
#   bash tests/texgif-tizen.sh
#   NV_TEXGIF_TEX=caminho/tex_cache.c bash tests/texgif-tizen.sh   # outra versao
#
# Passou quando imprime
#   RESULTADO gif_primeiro=1 gif_de_novo=0 gif_depois_de_reabrir=0 jpg=1 jpg_em_arquivo=0
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
EMCC="$EMSDK_DIR/upstream/emscripten/emcc"
[ -x "$EMCC" ] || { echo "texgif-tizen.sh: emcc nao encontrado em $EMSDK_DIR" >&2; exit 127; }
OUT="${TMPDIR:-/tmp}/nuvio-texgif"
mkdir -p "$OUT"
TESTE="$OUT/texgif_tizen.c"
# O teste inclui tex_cache.c; NV_TEXGIF_TEX troca qual (a medida do "antes").
sed "s|\"../src/tex_cache.c\"|\"${NV_TEXGIF_TEX:-$PWD/src/tex_cache.c}\"|" tests/texgif_tizen.c > "$TESTE"
REABRIR=-DNV_TESTE_REABRIR
[ -n "${NV_TEXGIF_TEX:-}" ] && REABRIR=""
sources=()
for s in src/*.c; do
  case "$s" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$s")
done
"$EMCC" "${sources[@]}" "$TESTE" -Isrc -O1 $REABRIR -o "$OUT/teste.js" \
  -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png","jpg"]' -sUSE_SDL_TTF=2 \
  -sUSE_LIBJPEG=1 -sUSE_ZLIB=1 -sWASM_BIGINT=0 -pthread -sPTHREAD_POOL_SIZE=2 \
  -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 -sENVIRONMENT=node,worker \
  -sEXIT_RUNTIME=1 -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  --pre-js tests/texgif-shim.js -Wno-deprecated-declarations -Wno-macro-redefined \
  > "$OUT/emcc.log" 2>&1 || { grep -E "error" "$OUT/emcc.log" | head -20; exit 1; }
# O fio de rede so olha a assinatura e o tamanho (> 512 B); nao decodifica.
python3 -c 'import sys; open(sys.argv[1], "wb").write(b"GIF89a" + bytes(4096))' "$OUT/anim.gif"
NV_TEXGIF_GIF="$OUT/anim.gif" NV_TEXGIF_JPG="$PWD/tests/amostra.jpg" \
  timeout 120 node "$OUT/teste.js" 2>&1 | grep RESULTADO
