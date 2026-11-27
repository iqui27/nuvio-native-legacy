#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu

PREFIX=${NUVIO_ASS_ROOT:-/opt/nuvio-ass}
BUILD=${NUVIO_ASS_BUILD:-${TMPDIR:-/tmp}/nuvio-ass-build}
CC=${CC:?CC must be the webOS cross compiler}
CXX=${CXX:-${CC%-gcc}-g++}
AR=${AR:-${CC%-gcc}-ar}
RANLIB=${RANLIB:-${CC%-gcc}-ranlib}
STRIP=${STRIP:-${CC%-gcc}-strip}
SYSROOT=${NUVIO_SYSROOT:?NUVIO_SYSROOT is required}
FREETYPE_VERSION=${FREETYPE_VERSION:-2.13.3}
FRIBIDI_VERSION=${FRIBIDI_VERSION:-1.0.16}
HARFBUZZ_VERSION=${HARFBUZZ_VERSION:-10.4.0}
LIBASS_VERSION=${LIBASS_VERSION:-0.17.5}

mkdir -p "$BUILD" "$PREFIX"
export AR RANLIB CC CXX
export CFLAGS="${CFLAGS:--O2 -fPIC}"
export LDFLAGS="${LDFLAGS:---sysroot=$SYSROOT}"
# The SDK ships its cross pkg-config alongside the compiler. Meson must use
# it so HarfBuzz can resolve the FreeType .pc file from the prefix being built.
PKG_CONFIG=${PKG_CONFIG:-$(command -v pkg-config || true)}
if [ -z "$PKG_CONFIG" ] || [ ! -x "$PKG_CONFIG" ]; then
  echo "pkg-config is required to build HarfBuzz" >&2
  exit 2
fi
export PKG_CONFIG

fetch() {
  url=$1; out=$2
  shift 2
  [ ! -f "$BUILD/$out" ] || return 0
  for candidate in "$url" "$@"; do
    if curl -fsSL --connect-timeout 20 --max-time 180 --retry 2 \
        "$candidate" -o "$BUILD/$out.part"; then
      mv "$BUILD/$out.part" "$BUILD/$out"
      return 0
    fi
    rm -f "$BUILD/$out.part"
  done
  echo "Could not download $out from any source" >&2
  return 1
}

fetch "https://downloads.sourceforge.net/project/freetype/freetype2/${FREETYPE_VERSION}/freetype-${FREETYPE_VERSION}.tar.xz" freetype.tar.xz \
  "https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz" \
  "https://ftp.osuosl.org/pub/blfs/conglomeration/freetype/freetype-${FREETYPE_VERSION}.tar.xz"
if [ "$FREETYPE_VERSION" = 2.13.3 ]; then
  printf '%s  %s\n' 0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289 "$BUILD/freetype.tar.xz" | sha256sum -c -
fi
tar -xf "$BUILD/freetype.tar.xz" -C "$BUILD"
cd "$BUILD/freetype-${FREETYPE_VERSION}"
./configure --host="${CC%-gcc}" --prefix="$PREFIX" --enable-static --disable-shared --without-zlib --without-bzip2 --without-png --without-brotli --without-harfbuzz
make -j2; make install

fetch "https://github.com/fribidi/fribidi/releases/download/v${FRIBIDI_VERSION}/fribidi-${FRIBIDI_VERSION}.tar.xz" fribidi.tar.xz
tar -xf "$BUILD/fribidi.tar.xz" -C "$BUILD"
cd "$BUILD/fribidi-${FRIBIDI_VERSION}"
./configure --host="${CC%-gcc}" --prefix="$PREFIX" --enable-static --disable-shared --without-glib
make -j2; make install

fetch "https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/harfbuzz-${HARFBUZZ_VERSION}.tar.xz" harfbuzz.tar.xz
tar -xf "$BUILD/harfbuzz.tar.xz" -C "$BUILD"
cd "$BUILD/harfbuzz-${HARFBUZZ_VERSION}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
printf '%s\n' "[binaries]" "c = '$CC'" "cpp = '$CXX'" "ar = '$AR'" "strip = '$STRIP'" "pkg-config = '$PKG_CONFIG'" "" "[properties]" "sys_root = '$SYSROOT'" "" "[built-in options]" "c_args = ['-I$PREFIX/include/freetype2']" "cpp_args = ['-I$PREFIX/include/freetype2']" "" "[host_machine]" "system = 'linux'" "kernel = 'linux'" "subsystem = 'linux'" "cpu_family = 'arm'" "cpu = 'armv7'" "endian = 'little'" > "$BUILD/ass-cross.ini"
meson setup build --cross-file "$BUILD/ass-cross.ini" --prefix="$PREFIX" -Ddefault_library=static -Dglib=disabled -Dgraphite=disabled -Dicu=disabled -Dcairo=disabled -Dtests=disabled -Ddocs=disabled -Dutilities=disabled -Dfreetype=enabled
meson compile -C build -j2; meson install -C build

# libass for the new subtitle engine (LG port, 25/11/2026): the same libass
# as the .tpk (tools/tpk.sh, NV_ASS_BASE=hybrid): Laskco/mpvNova-libass at
# NOVA_REV + tools/tpk/libass-hybrid/hybrid.patch (our libass-nv 0001-0004:
# NEON engine, blur threads, ASS_NV_API, split render) with its cflags.
# HYBRID_DIR holds hybrid.patch and cflags (tools/Dockerfile copies them).
# NUVIO_ASS_BASE=stock builds plain libass 0.17.5 as before.
ASS_BASE=${NUVIO_ASS_BASE:-hybrid}
NOVA_REV=${NOVA_REV:-b6209274415490615baa3f01f2409cb5facda674}
HYBRID_DIR=${NUVIO_ASS_HYBRID_DIR:-/tmp/libass-hybrid}
export CPPFLAGS="${CPPFLAGS:-} -I$PREFIX/include -I$PREFIX/include/fribidi -I$PREFIX/include/freetype2 -I$PREFIX/include/harfbuzz"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib"
if [ "$ASS_BASE" = hybrid ]; then
  [ -f "$HYBRID_DIR/hybrid.patch" ] && [ -f "$HYBRID_DIR/cflags" ] ||
    { echo "libass hybrid: $HYBRID_DIR/hybrid.patch and cflags are required" >&2; exit 2; }
  fetch "https://github.com/Laskco/mpvNova-libass/archive/$NOVA_REV.tar.gz" "mpvNova-libass-$NOVA_REV.tar.gz"
  rm -rf "$BUILD/libass-hybrid"; mkdir -p "$BUILD/libass-hybrid"
  tar -xzf "$BUILD/mpvNova-libass-$NOVA_REV.tar.gz" -C "$BUILD/libass-hybrid" --strip-components=1
  cd "$BUILD/libass-hybrid"
  patch -p1 -s --forward < "$HYBRID_DIR/hybrid.patch"
  ./autogen.sh >/dev/null
  # The cflags come after the image's -O2 (the last -O wins), as in tools/tpk/deps.sh.
  CFLAGS="$CFLAGS $(tr -d '\n' < "$HYBRID_DIR/cflags")" \
    ./configure --host="${CC%-gcc}" --prefix="$PREFIX" --enable-static --disable-shared --disable-fontconfig --disable-require-system-font-provider --disable-enca --disable-libunibreak --disable-asm
  make -j2; make install
  # What is inside, for tools/arm.sh and the [ass] tv log line.
  { echo "$NOVA_REV"; cat "$HYBRID_DIR/hybrid.patch" "$HYBRID_DIR/cflags"; } | sha256sum | cut -d' ' -f1 > "$PREFIX/lib/.libass-hybrid"
  grep -q "ass_nv_set_engine" "$PREFIX/include/ass/ass.h" || { echo "libass hybrid: ASS_NV_API missing from ass.h" >&2; exit 1; }
else
  fetch "https://github.com/libass/libass/releases/download/${LIBASS_VERSION}/libass-${LIBASS_VERSION}.tar.xz" libass.tar.xz
  tar -xf "$BUILD/libass.tar.xz" -C "$BUILD"
  cd "$BUILD/libass-${LIBASS_VERSION}"
  ./configure --host="${CC%-gcc}" --prefix="$PREFIX" --enable-static --disable-shared --disable-fontconfig --disable-require-system-font-provider --disable-enca --disable-libunibreak --disable-asm --enable-harfbuzz
  make -j2; make install
fi

test -f "$PREFIX/include/ass/ass.h"
test -f "$PREFIX/lib/libass.a"
echo "ASS ARM dependencies installed in $PREFIX"
