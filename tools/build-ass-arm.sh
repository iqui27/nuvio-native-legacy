#!/bin/sh
set -eu

PREFIX=${NUVIO_ASS_ROOT:-/opt/nuvio-ass}
BUILD=${NUVIO_ASS_BUILD:-/tmp/nuvio-ass-build}
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
  [ -f "$BUILD/$out" ] || curl -fsSL "$url" -o "$BUILD/$out"
}

fetch "https://download-mirror.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz" freetype.tar.xz
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

fetch "https://github.com/libass/libass/releases/download/${LIBASS_VERSION}/libass-${LIBASS_VERSION}.tar.xz" libass.tar.xz
tar -xf "$BUILD/libass.tar.xz" -C "$BUILD"
cd "$BUILD/libass-${LIBASS_VERSION}"
export CPPFLAGS="${CPPFLAGS:-} -I$PREFIX/include -I$PREFIX/include/fribidi -I$PREFIX/include/freetype2 -I$PREFIX/include/harfbuzz"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib"
./configure --host="${CC%-gcc}" --prefix="$PREFIX" --enable-static --disable-shared --disable-fontconfig --disable-require-system-font-provider --disable-enca --disable-libunibreak --disable-asm --enable-harfbuzz
make -j2; make install

test -f "$PREFIX/include/ass/ass.h"
test -f "$PREFIX/lib/libass.a"
echo "ASS ARM dependencies installed in $PREFIX"
