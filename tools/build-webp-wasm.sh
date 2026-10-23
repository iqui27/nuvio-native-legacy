#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# libwebp (decode + demux) para o alvo Tizen WebAssembly, para o WebP ANIMADO
# das capas de colecao (#141). Rode depois de ativar o emsdk; tools/tizen.sh
# liga NV_WEBP_ANIM quando acha o resultado em build/webp-wasm.
#
# POR QUE A MAO E NAO CMAKE: o Emscripten nao tem port de libwebp (ver
# tools/tizen.sh), e o decodificador e C puro sem configuracao que importe —
# as fontes de dec/, dsp/, utils/ e demux/ compiladas com emcc bastam. Nada de
# encoder, de SIMD nem de fio (WEBP_USE_THREAD fica desligado; gif.c pede
# use_threads=0 e ja roda num fio proprio).
#
# O WebP PARADO continua pelo navegador (src/webp.c, createImageBitmap num
# Worker): esta lib so entra no caminho da animacao, em src/gif.c.
set -eu

PREFIX=${NUVIO_WEBP_ROOT:-$(pwd)/build/webp-wasm}
BUILD=${NUVIO_WEBP_BUILD:-${TMPDIR:-/tmp}/nuvio-webp-wasm-build}
EMCC=${EMCC:-emcc}
EMAR=${EMAR:-emar}
LIBWEBP_VERSION=${LIBWEBP_VERSION:-1.4.0}
# sha256 do tarball oficial (storage.googleapis.com/downloads.webmproject.org).
LIBWEBP_SHA256=${LIBWEBP_SHA256:-61f873ec69e3be1b99535634340d5bde750b2e4447caa1db9f61be3fd49ab1e5}

mkdir -p "$BUILD" "$PREFIX/lib" "$PREFIX/include/webp"
TAR="$BUILD/libwebp-${LIBWEBP_VERSION}.tar.gz"
[ -f "$TAR" ] || curl -fsSL "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${LIBWEBP_VERSION}.tar.gz" -o "$TAR"
if command -v shasum >/dev/null 2>&1; then
  SUM=$(shasum -a 256 "$TAR" | cut -d' ' -f1)
else
  SUM=$(sha256sum "$TAR" | cut -d' ' -f1)
fi
[ "$SUM" = "$LIBWEBP_SHA256" ] || { echo "build-webp-wasm: sha256 inesperado ($SUM)" >&2; exit 2; }
rm -rf "$BUILD/libwebp-${LIBWEBP_VERSION}"
tar -xzf "$TAR" -C "$BUILD"
SRC="$BUILD/libwebp-${LIBWEBP_VERSION}"
OBJ="$BUILD/obj"
rm -rf "$OBJ"; mkdir -p "$OBJ"

# dsp/ traz tambem as rotinas do encoder (enc*.c, cost*.c, ssim*.c): nao sao
# chamadas pelo decode e o linker as descarta, mas compilar a pasta inteira
# evita manter uma lista de arquivos por versao.
i=0
for f in "$SRC"/src/dec/*.c "$SRC"/src/dsp/*.c "$SRC"/src/utils/*.c \
         "$SRC"/src/demux/demux.c "$SRC"/src/demux/anim_decode.c \
         "$SRC"/sharpyuv/*.c; do
  i=$((i + 1))
  "$EMCC" -O2 -pthread -I"$SRC" -I"$SRC/src" -c "$f" -o "$OBJ/$i-$(basename "$f" .c).o"
done
rm -f "$PREFIX/lib/libwebp.a"
"$EMAR" rcs "$PREFIX/lib/libwebp.a" "$OBJ"/*.o
for h in decode.h demux.h types.h mux_types.h; do
  cp "$SRC/src/webp/$h" "$PREFIX/include/webp/$h"
done
echo "build-webp-wasm: $PREFIX/lib/libwebp.a ($i objetos, libwebp $LIBWEBP_VERSION)"
