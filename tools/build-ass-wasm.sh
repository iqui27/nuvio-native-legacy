#!/bin/sh
set -eu

# Build the same libass stack for the Tizen WebAssembly target. Run after
# emsdk activation, or let CI call it before tools/tizen.sh.
PREFIX=${NUVIO_ASS_ROOT:-$(pwd)/build/ass-wasm}
BUILD=${NUVIO_ASS_BUILD:-/tmp/nuvio-ass-wasm-build}
EMCONFIGURE=${EMCONFIGURE:-emconfigure}
EMMAKE=${EMMAKE:-emmake}
EMCC=${EMCC:-emcc}
FREETYPE_VERSION=${FREETYPE_VERSION:-2.13.3}
FRIBIDI_VERSION=${FRIBIDI_VERSION:-1.0.16}
HARFBUZZ_VERSION=${HARFBUZZ_VERSION:-10.4.0}
LIBASS_VERSION=${LIBASS_VERSION:-0.17.5}

mkdir -p "$BUILD" "$PREFIX"
# libtool in FreeType 2.13 splits a --prefix containing spaces during install.
# Keep the real output directory expected by tizen.sh, but configure through a
# short /tmp symlink so install paths and generated pkg-config files stay valid.
INSTALL_PREFIX="$BUILD/ass-install-prefix"
if [ -L "$INSTALL_PREFIX" ]; then
  [ "$(readlink "$INSTALL_PREFIX")" = "$PREFIX" ] || {
    echo "unexpected existing install symlink: $INSTALL_PREFIX" >&2
    exit 2
  }
elif [ -e "$INSTALL_PREFIX" ]; then
  echo "install prefix path already exists and is not a symlink: $INSTALL_PREFIX" >&2
  exit 2
else
  ln -s "$PREFIX" "$INSTALL_PREFIX"
fi
if ! command -v meson >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
  python3 -m venv "$BUILD/build-tools"
  "$BUILD/build-tools/bin/python" -m pip install --disable-pip-version-check meson ninja
  PATH="$BUILD/build-tools/bin:$PATH"
  export PATH
fi
export CC="$EMCC" AR="emar" RANLIB="emranlib"
export CXX="${EMXX:-em++}"
# FreeType builds its apinames helper for the build host and executes it while
# generating headers; use the native compiler for that helper, Emscripten for
# the libraries.
export CC_BUILD="${CC_BUILD:-cc}"
export CFLAGS="${CFLAGS:--O2 -fPIC}"
export LDFLAGS="${LDFLAGS:--sSTRICT=0}"
PKG_CONFIG=${PKG_CONFIG:-$(command -v pkg-config || true)}
if [ -z "$PKG_CONFIG" ] || [ ! -x "$PKG_CONFIG" ]; then
  echo "pkg-config is required to build HarfBuzz" >&2
  exit 2
fi
export PKG_CONFIG

fetch() { [ -f "$BUILD/$2" ] || curl -fsSL "$1" -o "$BUILD/$2"; }

fetch "https://download-mirror.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz" freetype.tar.xz
tar -xf "$BUILD/freetype.tar.xz" -C "$BUILD"
cd "$BUILD/freetype-${FREETYPE_VERSION}"
BUILD_TRIPLET=$(./builds/unix/config.guess)
CC_BUILD="$CC_BUILD" $EMCONFIGURE ./configure --build="$BUILD_TRIPLET" --host=wasm32-unknown-emscripten --prefix="$INSTALL_PREFIX" --enable-static --disable-shared --without-zlib --without-bzip2 --without-png --without-brotli --without-harfbuzz
# An interrupted earlier build can leave the apinames JS/WASM helper in place;
# remove it so FreeType rebuilds that one host tool with CC_BUILD.
rm -f "$BUILD/freetype-${FREETYPE_VERSION}/objs/apinames" "$BUILD/freetype-${FREETYPE_VERSION}/objs/apinames.wasm"
$EMMAKE make -j2; $EMMAKE make install

fetch "https://github.com/fribidi/fribidi/releases/download/v${FRIBIDI_VERSION}/fribidi-${FRIBIDI_VERSION}.tar.xz" fribidi.tar.xz
tar -xf "$BUILD/fribidi.tar.xz" -C "$BUILD"
cd "$BUILD/fribidi-${FRIBIDI_VERSION}"
$EMCONFIGURE ./configure --host=wasm32-unknown-emscripten --prefix="$INSTALL_PREFIX" --enable-static --disable-shared --without-glib
$EMMAKE make -j2; $EMMAKE make install

fetch "https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/harfbuzz-${HARFBUZZ_VERSION}.tar.xz" harfbuzz.tar.xz
tar -xf "$BUILD/harfbuzz.tar.xz" -C "$BUILD"
cd "$BUILD/harfbuzz-${HARFBUZZ_VERSION}"
export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$INSTALL_PREFIX/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig"
printf '%s\n' "[binaries]" "c = '$EMCC'" "cpp = '$CXX'" "ar = 'emar'" "strip = 'emstrip'" "pkg-config = '$PKG_CONFIG'" "" "[built-in options]" "c_args = ['-I$INSTALL_PREFIX/include/freetype2']" "cpp_args = ['-I$INSTALL_PREFIX/include/freetype2']" "" "[host_machine]" "system = 'emscripten'" "cpu_family = 'wasm32'" "cpu = 'wasm32'" "endian = 'little'" > "$BUILD/wasm-cross.ini"
meson setup build --cross-file "$BUILD/wasm-cross.ini" --prefix="$INSTALL_PREFIX" -Ddefault_library=static -Dglib=disabled -Dgraphite=disabled -Dicu=disabled -Dcairo=disabled -Dtests=disabled -Ddocs=disabled -Dutilities=disabled -Dfreetype=enabled
meson compile -C build -j2; meson install -C build

fetch "https://github.com/libass/libass/releases/download/${LIBASS_VERSION}/libass-${LIBASS_VERSION}.tar.xz" libass.tar.xz
tar -xf "$BUILD/libass.tar.xz" -C "$BUILD"
cd "$BUILD/libass-${LIBASS_VERSION}"
export CPPFLAGS="${CPPFLAGS:-} -I$INSTALL_PREFIX/include -I$INSTALL_PREFIX/include/fribidi -I$INSTALL_PREFIX/include/freetype2 -I$INSTALL_PREFIX/include/harfbuzz"
export LDFLAGS="$LDFLAGS -L$INSTALL_PREFIX/lib"
$EMCONFIGURE ./configure --host=wasm32-unknown-emscripten --prefix="$INSTALL_PREFIX" --enable-static --disable-shared --disable-fontconfig --disable-require-system-font-provider --disable-enca --disable-libunibreak --disable-asm --enable-harfbuzz
$EMMAKE make -j2; $EMMAKE make install

test -f "$PREFIX/include/ass/ass.h"
test -f "$PREFIX/lib/libass.a"
echo "ASS WASM dependencies installed in $PREFIX"
