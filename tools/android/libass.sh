#!/usr/bin/env bash
# libass for the Android .apk: the same hybrid libass as the .tpk
# (Laskco/mpvNova-libass at tools/tpk.sh's NOVA_REV + tools/tpk/libass-hybrid/
# hybrid.patch + its cflags), with fribidi and HarfBuzz, static, per ABI.
#
# FreeType is NOT built here. libmain.so already carries the FreeType that
# SDL2_ttf vendors (SDL2_ttf-2.22.0/external/freetype, built by the app's
# CMake). libass and HarfBuzz compile against those headers and their FT_*
# symbols are resolved by that one copy at the final link (same rule as the
# .tpk: one FreeType for the app and libass).
#
# No fontconfig (fonts come from the app: assrender.c ass_set_fonts with a
# file and the extracted font folder, plus /system/fonts as a last resort), no
# libunibreak, no enca.
#   armv7: asm off, mpvNova's NEON intrinsics engine (cflags ASS_NV_ARMV7_NOVA=1).
#   arm64: libass's own aarch64 asm.
#
# Out: $CACHE/prefix/<abi>/libass/{lib,include}. Idempotent per ABI: the stamp
# is NOVA_REV + hybrid.patch + cflags + this script + the NDK.
# Use: bash tools/android/libass.sh [abi...]   FORCAR=1 rebuilds.
# NV_ANDROID_LIBASS_HOST=1 builds for this computer instead (script check only).
set -euo pipefail
cd "$(dirname "$0")/../.."
RAIZ="$PWD"

FRIBIDI_V=1.0.16
HB_V=10.4.0
NOVA_REV=$(sed -n 's/^NOVA_REV=\([0-9a-f]*\).*/\1/p' tools/tpk.sh | head -1)
[ -n "$NOVA_REV" ] || { echo "libass.sh: NOVA_REV not found in tools/tpk.sh" >&2; exit 1; }

CACHE="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
SRC="$CACHE/src"; PREFIX="$CACHE/prefix"
FT_INC="$SRC/SDL2_ttf-2.22.0/external/freetype/include"
[ -f "$FT_INC/ft2build.h" ] || { echo "libass.sh: $FT_INC missing (tools/android.sh downloads SDL2_ttf first)" >&2; exit 1; }
JOBS="${NUVIO_ANDROID_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

HOSTMODE="${NV_ANDROID_LIBASS_HOST:-0}"
if [ "$HOSTMODE" = 1 ]; then
  ABIS=(host); NDKSIG=host
else
  case "$(uname -s)" in
    Darwin) HOST=darwin-x86_64; SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
    Linux) HOST=linux-x86_64; SDK_DEFAULT="$HOME/Android/Sdk" ;;
    *) echo "libass.sh: macOS or Linux with the Android NDK" >&2; exit 1 ;;
  esac
  SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$SDK_DEFAULT}}"
  NDK="${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}"
  TC="$NDK/toolchains/llvm/prebuilt/$HOST"
  [ -x "$TC/bin/clang" ] || { echo "libass.sh: NDK not found in $NDK" >&2; exit 1; }
  NDKSIG=$(cksum < "$NDK/source.properties")
  ABIS=("$@"); [ ${#ABIS[@]} -gt 0 ] || ABIS=(arm64-v8a armeabi-v7a)
fi
command -v autoreconf >/dev/null || { echo "libass.sh: autoreconf missing (autoconf automake libtool)" >&2; exit 1; }

mkdir -p "$SRC"
baixar() {  # baixar <file> <url> <dir>
  [ -d "$SRC/$3" ] && return 0
  [ -f "$SRC/$1" ] || { echo ">> downloading $1"; curl -fL --retry 6 --retry-delay 10 --retry-all-errors -o "$SRC/$1.part" "$2" && mv "$SRC/$1.part" "$SRC/$1"; }
  tar -xf "$SRC/$1" -C "$SRC"
  [ -d "$SRC/$3" ] || { echo "libass.sh: $3 not in $1" >&2; exit 1; }
}
baixar "fribidi-$FRIBIDI_V.tar.xz" "https://github.com/fribidi/fribidi/releases/download/v$FRIBIDI_V/fribidi-$FRIBIDI_V.tar.xz" "fribidi-$FRIBIDI_V"
baixar "harfbuzz-$HB_V.tar.xz" "https://github.com/harfbuzz/harfbuzz/releases/download/$HB_V/harfbuzz-$HB_V.tar.xz" "harfbuzz-$HB_V"
[ -f "$SRC/mpvNova-libass-$NOVA_REV.tar.gz" ] ||
  { curl -fsSL --retry 6 --retry-delay 10 --retry-all-errors "https://github.com/Laskco/mpvNova-libass/archive/$NOVA_REV.tar.gz" -o "$SRC/nova.part" &&
    mv "$SRC/nova.part" "$SRC/mpvNova-libass-$NOVA_REV.tar.gz"; }

STAMP=$( { echo "$NOVA_REV $FRIBIDI_V $HB_V $NDKSIG"; cat tools/tpk/libass-hybrid/hybrid.patch tools/tpk/libass-hybrid/cflags "${BASH_SOURCE[0]}"; } |
  { sha256sum 2>/dev/null || shasum -a 256; } | cut -d' ' -f1)

for abi in "${ABIS[@]}"; do
  P="$PREFIX/$abi/libass"
  if [ "${FORCAR:-0}" != 1 ] && [ "$(cat "$P/.stamp" 2>/dev/null)" = "$STAMP" ] && [ -f "$P/lib/libass.a" ]; then
    echo ">> libass $abi: up to date"; continue
  fi
  echo ">> libass $abi: mpvNova-libass ${NOVA_REV:0:8} + hybrid.patch"
  rm -rf "$P"; mkdir -p "$P/lib/pkgconfig" "$P/include"
  W=$(mktemp -d "${TMPDIR:-/tmp}/nv-libass-$abi.XXXXXX")
  trap 'rm -rf "$W"' EXIT

  ASM=--disable-asm
  ABICF="-O3 -fPIC -fvisibility=hidden"
  if [ "$HOSTMODE" = 1 ]; then
    CC=cc; CXX=c++; AR=ar; RANLIB=ranlib; HOSTARG=""
  else
    case "$abi" in
      arm64-v8a) T=aarch64-linux-android24; ASM=--enable-asm ;;
      armeabi-v7a) T=armv7a-linux-androideabi24; ABICF="$ABICF -mfpu=neon" ;;
      *) echo "libass.sh: ABI $abi not supported (Phase A: ARM only)" >&2; exit 1 ;;
    esac
    CC="$TC/bin/$T-clang"; CXX="$TC/bin/$T-clang++"; AR="$TC/bin/llvm-ar"; RANLIB="$TC/bin/llvm-ranlib"
    HOSTARG="--host=${T%24}"
  fi
  export PKG_CONFIG_PATH="$P/lib/pkgconfig" PKG_CONFIG_LIBDIR="$P/lib/pkgconfig"

  # FreeType: headers only (see the top). The .pc has no Libs on purpose.
  cat > "$P/lib/pkgconfig/freetype2.pc" <<PC
Name: FreeType 2
Description: SDL2_ttf's vendored FreeType (linked by libmain.so)
Version: 26.1.20
Libs:
Cflags: -I$FT_INC
PC

  # fribidi
  cp -a "$SRC/fribidi-$FRIBIDI_V" "$W/fribidi"
  ( cd "$W/fribidi" && CC="$CC" AR="$AR" RANLIB="$RANLIB" CFLAGS="$ABICF" \
      ./configure $HOSTARG -q --prefix="$P" --enable-static --disable-shared --without-glib --disable-docs >/dev/null &&
    make -j"$JOBS" -C lib >/dev/null && make -C lib install >/dev/null && make install-pkgconfigDATA >/dev/null ) ||
    { echo "libass.sh: fribidi $abi failed" >&2; exit 1; }

  # HarfBuzz: one object per .cc of harfbuzz.cc, no exceptions/RTTI/static
  # guards, so no C++ runtime is needed in libmain.so (same as the .tpk).
  HB="$SRC/harfbuzz-$HB_V"
  mkdir -p "$W/hb/OT/Var/VARC"
  grep -oE '#include "[A-Za-z0-9/-]+\.cc"' "$HB/src/harfbuzz.cc" | cut -d'"' -f2 > "$W/hb/lista"
  ( cd "$W/hb" && xargs -P "$JOBS" -I{} "$CXX" $ABICF -std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
      -fvisibility-inlines-hidden -DHAVE_FREETYPE -DHAVE_PTHREAD -DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR -DNDEBUG \
      -I"$HB/src" -I"$FT_INC" -c "$HB/src/{}" -o {}.o < lista ) || { echo "libass.sh: harfbuzz $abi failed" >&2; exit 1; }
  "$AR" rcs "$P/lib/libharfbuzz.a" $(find "$W/hb" -name '*.o')
  "$RANLIB" "$P/lib/libharfbuzz.a"
  mkdir -p "$P/include/harfbuzz" && cp "$HB"/src/hb*.h "$P/include/harfbuzz/"
  cat > "$P/lib/pkgconfig/harfbuzz.pc" <<PC
prefix=$P
Name: harfbuzz
Description: HarfBuzz text shaping library
Version: $HB_V
Requires: freetype2
Libs: -L\${prefix}/lib -lharfbuzz
Cflags: -I\${prefix}/include/harfbuzz
PC

  # libass hybrid (the .tpk's tree, patch and cflags; -mtune only on ARM).
  NVCF=$(tr -d '\n' < tools/tpk/libass-hybrid/cflags)
  [ "$HOSTMODE" = 1 ] && NVCF=$(printf '%s' "$NVCF" | sed 's/-mtune=[^ ]*//')
  mkdir -p "$W/ass"
  tar xzf "$SRC/mpvNova-libass-$NOVA_REV.tar.gz" -C "$W/ass" --strip-components=1
  patch -d "$W/ass" -p1 -s --forward < tools/tpk/libass-hybrid/hybrid.patch || { echo "libass.sh: hybrid patch failed" >&2; exit 1; }
  ( cd "$W/ass" && ./autogen.sh >/dev/null 2>&1 ) || { echo "libass.sh: autogen failed" >&2; exit 1; }
  ( cd "$W/ass" && CC="$CC" AR="$AR" RANLIB="$RANLIB" CFLAGS="$ABICF $NVCF" \
      CPPFLAGS="-I$P/include" LDFLAGS="-L$P/lib" \
      ./configure $HOSTARG -q --prefix="$P" --enable-static --disable-shared --disable-fontconfig \
        --disable-require-system-font-provider --disable-libunibreak $ASM --enable-harfbuzz >/dev/null &&
    make -j"$JOBS" >/dev/null && make install >/dev/null ) || { echo "libass.sh: libass $abi failed (see $W/ass/config.log)" >&2; trap - EXIT; exit 1; }
  grep -q 'ass_nv_get_engine' "$P/include/ass/ass.h" || { echo "libass.sh: ass.h without ASS_NV_API" >&2; exit 1; }
  printf '%s' "$STAMP" > "$P/.stamp"
  rm -rf "$W"; trap - EXIT
  echo ">> libass $abi: $(du -h "$P/lib/libass.a" | cut -f1) libass.a"
done
