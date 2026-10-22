#!/usr/bin/env bash
# Dependencias nativas do porte Android: libcurl.so (curl + mbedTLS estatica),
# libjpeg.so (libjpeg-turbo, ABI 6.2) e libwebp.so, para arm64-v8a e armeabi-v7a.
# O nucleo faz dlopen delas (rede.c, jpegrapido.c, webp.c), entao precisam ter
# SONAME sem versao: o APK so empacota lib*.so.
# Idempotente: pula builds com a mesma receita e NDK. Saida: ~/.cache/nuvio-android/prefix/<abi>/{lib,include}
# Uso: bash tools/android/deps.sh [abi...]     FORCAR=1 reconstroi tudo.
set -euo pipefail

MBEDTLS_V=3.6.7
CURL_V=8.22.0
JPEG_V=3.1.4.1
WEBP_V=1.6.0

case "$(uname -s)" in
  Darwin) HOST=darwin-x86_64; SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
  Linux) HOST=linux-x86_64; SDK_DEFAULT="$HOME/Android/Sdk" ;;
  *) echo "deps.sh: execute no macOS ou Linux com o Android NDK" >&2; exit 1 ;;
esac
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$SDK_DEFAULT}}"
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}"
TC="$NDK/toolchains/llvm/prebuilt/$HOST"
CMAKE_SDK="$SDK/cmake/3.22.1/bin"
if [ -x "$CMAKE_SDK/cmake" ]; then CMAKE="$CMAKE_SDK/cmake"; NINJA="$CMAKE_SDK/ninja"; else CMAKE="$(command -v cmake)"; NINJA="$(command -v ninja)"; fi
[ -f "$NDK/build/cmake/android.toolchain.cmake" ] || { echo "NDK nao encontrado em $NDK" >&2; exit 1; }
[ -x "$CMAKE" ] && [ -x "$NINJA" ] || { echo "cmake/ninja ausentes" >&2; exit 1; }
READELF="$TC/bin/llvm-readelf"; NM="$TC/bin/llvm-nm"

CACHE="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
SRC="$CACHE/src"; BUILD="$CACHE/build"; PREFIX="$CACHE/prefix"
mkdir -p "$SRC" "$BUILD"
# Uma .so existente pode ter sido ligada com flags antigas. A assinatura
# invalida tambem caches locais, alem da chave de cache da build no CI.
ASSINATURA="$(cksum < "${BASH_SOURCE[0]}") $(cksum < "$NDK/source.properties")"
JOBS="${NUVIO_ANDROID_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
ABIS=("$@"); [ ${#ABIS[@]} -gt 0 ] || ABIS=(arm64-v8a armeabi-v7a)
triple() {
  case "$1" in
    arm64-v8a) echo aarch64-linux-android24 ;;
    armeabi-v7a) echo armv7a-linux-androideabi24 ;;
    x86_64) echo x86_64-linux-android24 ;;
    x86) echo i686-linux-android24 ;;
    *) echo "deps.sh: ABI desconhecida: $1" >&2; return 1 ;;
  esac
}
for abi in "${ABIS[@]}"; do triple "$abi" >/dev/null; done

baixar() {  # baixar <arquivo> <url> ; extrai em $SRC/<dir>
  local arq="$1" url="$2" dir="$3"
  [ -d "$SRC/$dir" ] && return 0
  [ -f "$SRC/$arq" ] || { echo ">> baixando $arq"; curl -fL --retry 3 -o "$SRC/$arq.part" "$url" && mv "$SRC/$arq.part" "$SRC/$arq"; }
  echo ">> extraindo $arq"; tar -xf "$SRC/$arq" -C "$SRC"
  [ -d "$SRC/$dir" ] || { echo "diretorio $dir nao saiu de $arq" >&2; exit 1; }
}
baixar "mbedtls-$MBEDTLS_V.tar.bz2" "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_V/mbedtls-$MBEDTLS_V.tar.bz2" "mbedtls-$MBEDTLS_V"
baixar "curl-$CURL_V.tar.xz" "https://curl.se/download/curl-$CURL_V.tar.xz" "curl-$CURL_V"
baixar "libjpeg-turbo-$JPEG_V.tar.gz" "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$JPEG_V.tar.gz" "libjpeg-turbo-$JPEG_V"
baixar "libwebp-$WEBP_V.tar.gz" "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$WEBP_V.tar.gz" "libwebp-$WEBP_V"

# SONAME sem versao: ultimo -soname na linha vence no lld.
cmk() {  # cmk <abi> <srcdir> <builddir> <soname|""> [args...]
  local abi="$1" s="$2" b="$3" so="$4"; shift 4
  local lf="-Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"
  [ -n "$so" ] && lf="$lf -Wl,-soname,$so"
  rm -rf "$b"
  "$CMAKE" -S "$s" -B "$b" -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-24 -DANDROID_STL=none \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="${CFLAGS_EXTRA:--fPIC -fvisibility=hidden}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_SHARED_LINKER_FLAGS="$lf" \
    -DCMAKE_MODULE_LINKER_FLAGS="$lf" -DCMAKE_EXE_LINKER_FLAGS="$lf" \
    -DCMAKE_INSTALL_PREFIX="${CMK_PREFIX:-$PREFIX/$abi}" "$@" >"$b.log" 2>&1 || { tail -30 "$b.log"; exit 1; }
  "$CMAKE" --build "$b" -j"$JOBS" >>"$b.log" 2>&1 || { tail -40 "$b.log"; exit 1; }
  "$CMAKE" --install "$b" >>"$b.log" 2>&1
  rm -rf "$b"
}
reduzir() { "$TC/bin/llvm-strip" --strip-unneeded "$1"; }

# Instala a .so real (sem symlinks de versao) como lib<nome>.so
so_unica() {  # so_unica <prefix-lib> <nome>
  local d="$1" n="$2" real
  real="$(ls "$d"/lib$n.so* 2>/dev/null | while read -r f; do [ -L "$f" ] || echo "$f"; done | head -1)"
  [ -n "$real" ] || { echo "lib$n.so nao instalada" >&2; exit 1; }
  if [ "$real" != "$d/lib$n.so" ]; then cp "$real" "$d/lib$n.so.tmp"; rm -f "$d"/lib$n.so*; mv "$d/lib$n.so.tmp" "$d/lib$n.so"; fi
}

for abi in "${ABIS[@]}"; do
  P="$PREFIX/$abi"; mkdir -p "$P/lib" "$P/include"
  if [ -n "${FORCAR:-}" ] || [ "$(cat "$P/android-deps.build" 2>/dev/null || true)" != "$ASSINATURA" ]; then
    rm -f "$P/lib/libcurl.so" "$P/lib/libjpeg.so" "$P/lib/libwebp.so" \
      "$P/lib/libmbedtls.a" "$P/lib/libmbedx509.a" "$P/lib/libmbedcrypto.a" "$P/android-deps.build"
  fi

  # O nucleo faz TLS em 4 fios ao mesmo tempo (tex_cache, descoberta, sync).
  # Sem MBEDTLS_THREADING_C o estado global do PSA (TLS 1.3 do mbedTLS 3.6) e
  # disputado e o Scudo do Android 11+ derruba o app com "race on chunk header"
  # dentro da libcurl (TCL Smart TV Pro, 30/09/2026, 3 quedas). Liga no proprio
  # mbedtls_config.h, e nao por -D, para curl e mbedTLS verem as MESMAS structs.
  CFG="$SRC/mbedtls-$MBEDTLS_V/include/mbedtls/mbedtls_config.h"
  if grep -q '^//#define MBEDTLS_THREADING_C' "$CFG"; then
    sed -i.bak -e 's|^//#define MBEDTLS_THREADING_C$|#define MBEDTLS_THREADING_C|' \
              -e 's|^//#define MBEDTLS_THREADING_PTHREAD$|#define MBEDTLS_THREADING_PTHREAD|' "$CFG"
    rm -f "$CFG.bak"
    rm -f "$PREFIX"/*/lib/libmbedtls.a "$PREFIX"/*/lib/libcurl.so
  fi
  # ENTROPIA DE /dev/urandom (#266 Shield, #332 BRAVIA). O mbedTLS 3.6 so usa
  # getrandom() com __GLIBC__; no bionic cai no MBEDTLS_PLATFORM_DEV_RANDOM, que
  # por padrao e "/dev/random". Em kernel < 5.6 (Android 9-11 de TV: 3.18/4.4/4.9)
  # /dev/random BLOQUEIA quando a estimativa de entropia esta baixa, e e la que
  # psa_crypto_init/ctr_drbg_seed (curl_global_init) leem. /dev/urandom nunca
  # bloqueia num aparelho ja ligado; e o que o proprio bionic (arc4random) usa.
  if ! grep -q '^#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/urandom"$' "$CFG"; then
    sed -i.bak -e 's|^//#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/random"$|#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/urandom"|' "$CFG"
    rm -f "$CFG.bak"
    grep -q '^#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/urandom"$' "$CFG" || { echo "deps.sh: nao achei MBEDTLS_PLATFORM_DEV_RANDOM em $CFG" >&2; exit 1; }
    rm -f "$PREFIX"/*/lib/libmbedtls.a "$PREFIX"/*/lib/libcurl.so
  fi

  if [ ! -f "$P/lib/libmbedtls.a" ]; then
    echo "== mbedtls $MBEDTLS_V $abi"
    cmk "$abi" "$SRC/mbedtls-$MBEDTLS_V" "$BUILD/mbedtls-$abi" "" \
      -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON \
      -DMBEDTLS_FATAL_WARNINGS=OFF -DGEN_FILES=OFF
  fi

  if [ ! -f "$P/lib/libcurl.so" ]; then
    echo "== curl $CURL_V $abi"
    cmk "$abi" "$SRC/curl-$CURL_V" "$BUILD/curl-$abi" "libcurl.so" \
      -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
      -DCMAKE_PLATFORM_NO_VERSIONED_SONAME=ON -DCMAKE_FIND_ROOT_PATH="$P" \
      -DHTTP_ONLY=ON -DCURL_ENABLE_SSL=ON -DCURL_USE_MBEDTLS=ON -DCURL_USE_OPENSSL=OFF \
      -DMBEDTLS_INCLUDE_DIR="$P/include" -DMBEDTLS_LIBRARY="$P/lib/libmbedtls.a" \
      -DMBEDX509_LIBRARY="$P/lib/libmbedx509.a" -DMBEDCRYPTO_LIBRARY="$P/lib/libmbedcrypto.a" \
      -DCURL_USE_LIBPSL=OFF -DUSE_LIBIDN2=OFF -DUSE_NGHTTP2=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_USE_LIBSSH=OFF \
      -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DCURL_ZLIB=ON -DUSE_APPLE_IDN=OFF \
      -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DCURL_DISABLE_COOKIES=OFF \
      -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none -DCURL_CA_FALLBACK=OFF -DCURL_USE_GSSAPI=OFF \
      -DENABLE_CURL_MANUAL=OFF -DENABLE_THREADED_RESOLVER=ON
    so_unica "$P/lib" curl; reduzir "$P/lib/libcurl.so"
  fi

  if [ ! -f "$P/lib/libjpeg.so" ]; then
    echo "== libjpeg-turbo $JPEG_V $abi"
    CFLAGS_EXTRA="-fPIC" cmk "$abi" "$SRC/libjpeg-turbo-$JPEG_V" "$BUILD/jpeg-$abi" "libjpeg.so" \
      -DENABLE_SHARED=ON -DENABLE_STATIC=OFF -DWITH_JPEG7=OFF -DWITH_JPEG8=OFF -DWITH_TURBOJPEG=OFF \
      -DWITH_JAVA=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DCMAKE_PLATFORM_NO_VERSIONED_SONAME=ON \
      -DCMAKE_ASM_FLAGS="--target=$(triple "$abi")"
    so_unica "$P/lib" jpeg; reduzir "$P/lib/libjpeg.so"
  fi

  if [ ! -f "$P/lib/libwebp.so" ]; then
    echo "== libwebp $WEBP_V $abi"
    # libwebp.so unica; webp.c so pede WebPGetInfo/WebPDecodeRGBA/WebPFree/WebPDecode (nada de demux).
    WS="$CACHE/webp-static/$abi"; rm -rf "$WS"
    CFLAGS_EXTRA="-fPIC" CMK_PREFIX="$WS" cmk "$abi" "$SRC/libwebp-$WEBP_V" "$BUILD/webp-$abi" "" \
      -DBUILD_SHARED_LIBS=OFF -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
      -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF \
      -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DWEBP_BUILD_LIBWEBPMUX=OFF
    # .so unica com sharpyuv dentro (o dlopen so acha libwebp.so); a .a fica privada
    # para nao colidir com a libwebp.a que o SDL_image usa em $P/lib.
    TRIPLE=$(triple "$abi")
    "$TC/bin/clang" --target=$TRIPLE -shared -o "$P/lib/libwebp.so" -Wl,-soname,libwebp.so \
      -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384 \
      -Wl,--whole-archive "$WS/lib/libwebp.a" "$WS/lib/libsharpyuv.a" -Wl,--no-whole-archive -lm
    mkdir -p "$P/include/webp"; cp "$WS"/include/webp/*.h "$P/include/webp/" 2>/dev/null || true
    rm -rf "$WS"; reduzir "$P/lib/libwebp.so"
  fi
done

# restos de builds antigas (libwebp agora e uma .so unica) e strip idempotente
for abi in "${ABIS[@]}"; do
  rm -f "$PREFIX/$abi/lib/libsharpyuv.so" "$PREFIX/$abi/lib/libwebpdecoder.so" "$PREFIX/$abi/lib/libwebpdemux.so"
  for n in curl jpeg webp; do [ -f "$PREFIX/$abi/lib/lib$n.so" ] && reduzir "$PREFIX/$abi/lib/lib$n.so"; done
done
rm -rf "$CACHE/webp-static"; rmdir "$BUILD" 2>/dev/null || true

# ---- conferencia ----
falhou=0
conferir() {  # conferir <abi> <lib> <sym...>
  local abi="$1" n="$2"; shift 2
  local f="$PREFIX/$abi/lib/$n" dyn faltam=""
  [ -f "$f" ] || { echo "FALTA $f"; falhou=1; return; }
  echo "$abi $n: $(du -k "$f" | cut -f1) KB, $("$READELF" -d "$f" | grep -E 'SONAME' | sed 's/.*\[\(.*\)\]/SONAME \1/')"
  echo "   NEEDED: $("$READELF" -d "$f" | grep NEEDED | sed 's/.*\[\(.*\)\]/\1/' | tr '\n' ' ')"
  dyn="$("$NM" -D --defined-only "$f" | awk '{print $NF}' | sed 's/@.*//')"
  for s in "$@"; do grep -qx "$s" <<< "$dyn" || faltam="$faltam $s"; done
  [ -z "$faltam" ] && echo "   simbolos OK ($#)" || { echo "   FALTAM:$faltam"; falhou=1; }
}
for abi in "${ABIS[@]}"; do
  conferir "$abi" libcurl.so curl_easy_init curl_easy_setopt curl_easy_perform curl_easy_cleanup curl_global_init \
    curl_slist_append curl_slist_free_all curl_easy_getinfo curl_easy_reset curl_easy_strerror
  # #266/#332: a entropia tem de vir de /dev/urandom (ver MBEDTLS_PLATFORM_DEV_RANDOM acima).
  dev="$(strings "$PREFIX/$abi/lib/libcurl.so" | grep -E '^/dev/u?random$' || true)"
  if [ "$dev" != "/dev/urandom" ]; then
    echo "   $abi libcurl.so le /dev/random (bloqueia em kernel antigo)"; falhou=1
  fi
  conferir "$abi" libjpeg.so jpeg_std_error jpeg_CreateDecompress jpeg_stdio_src jpeg_read_header jpeg_start_decompress \
    jpeg_read_scanlines jpeg_finish_decompress jpeg_destroy_decompress jpeg_calc_output_dimensions
  conferir "$abi" libwebp.so WebPGetInfo WebPDecodeRGBA WebPFree WebPInitDecoderConfigInternal WebPDecode WebPFreeDecBuffer
done
[ $falhou -eq 0 ] || { echo "deps.sh: FALHOU"; exit 1; }
for abi in "${ABIS[@]}"; do printf '%s\n' "$ASSINATURA" > "$PREFIX/$abi/android-deps.build"; done
echo "deps.sh: OK"
