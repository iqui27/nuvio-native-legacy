# Dependencias estaticas do .tpk. Roda DENTRO de nuvio-tpk-sdk (tools/tpk/Dockerfile),
# com o cache montado em /w e os fontes em /w/src (tools/tpk.sh baixa).
set -e
P=/w/prefix; mkdir -p $P
cd /w/src/SDL2-2.30.9
[ -f $P/lib/libSDL2.a ] || { ./configure -q --prefix=$P --enable-static --disable-shared --disable-audio --disable-video-x11 --disable-video-wayland --disable-video-kmsdrm --disable-video-vulkan --disable-video-opengl --disable-video-opengles --disable-joystick --disable-haptic --disable-sensor --disable-hidapi --disable-pulseaudio --disable-alsa --disable-dbus --disable-ime --disable-ibus --disable-fcitx --disable-libudev --disable-video-rpi --disable-render --disable-sndio --disable-pipewire --disable-jack --disable-esd --disable-arts --disable-nas --disable-oss && make -j6 >/dev/null && make install >/dev/null; } || exit 1
export PKG_CONFIG_PATH=$P/lib/pkgconfig PATH=$P/bin:$PATH
# libwebp ESTATICA: o Tizen nao expoe libwebp a apps, entao o dlopen do webp.c
# falha na TV e, sem WebP no SDL_image, toda arte .webp sumia (logs de 28/09).
cd /w/src/libwebp-1.4.0
[ -f $P/lib/libwebp.a ] || { ./configure -q --prefix=$P --enable-static --disable-shared --enable-libwebpdemux --disable-libwebpmux --disable-libwebpdecoder --disable-gl --disable-sdl --disable-png --disable-jpeg --disable-tiff --disable-gif --disable-wic && make -j6 >/dev/null && make install >/dev/null; } || exit 1
cd /w/src/SDL2_image-2.8.2
# O carimbo diz que esta copia ja tem WebP; a antiga (sem) e refeita.
[ -f $P/lib/libSDL2_image.a ] && [ -f $P/lib/.sdlimage-webp ] || { make distclean >/dev/null 2>&1; ./configure -q --prefix=$P --enable-static --disable-shared --enable-stb-image --disable-jpg-shared --disable-png-shared --enable-webp --disable-webp-shared --disable-avif --disable-jxl --disable-tif --disable-qoi && make -j6 >/dev/null && make install >/dev/null && touch $P/lib/.sdlimage-webp; } || exit 1
# FreeType UNICO (#ass-tpk): o libass e o SDL2_ttf usam a MESMA FreeType, estatica.
# Com a embutida do SDL2_ttf (--enable-freetype-builtin) a libnuvio.so teria duas
# copias dos simbolos FT_* no link e o libass chamaria uma versao diferente da que
# o resto do app usa. Sem zlib/png/bz2/brotli/harfbuzz: o app so abre TTF/OTF.
cd /w/src/freetype-2.13.3
[ -f $P/lib/libfreetype.a ] || { ./configure -q --prefix=$P --enable-static --disable-shared --without-zlib --without-bzip2 --without-png --without-brotli --without-harfbuzz && make -j6 >/dev/null && make install >/dev/null; } || exit 1
cd /w/src/SDL2_ttf-2.22.0
[ -f $P/lib/libSDL2_ttf.a ] && [ -f $P/lib/.ttf-freetype-externo ] || { make distclean >/dev/null 2>&1; touch aclocal.m4; touch configure Makefile.in; ./configure -q --prefix=$P --enable-static --disable-shared --disable-freetype-builtin --disable-harfbuzz && make -j6 >/dev/null && make install >/dev/null && touch $P/lib/.ttf-freetype-externo; } || exit 1
# libass (mesma receita do webOS, tools/build-ass-arm.sh): sem fontconfig, sem
# libunibreak, sem asm. As fontes vem do app (assrender.c: ass_set_fonts com
# arquivo), nao do sistema.
cd /w/src/fribidi-1.0.16
[ -f $P/lib/libfribidi.a ] || { ./configure -q --prefix=$P --enable-static --disable-shared --without-glib && make -j6 >/dev/null && make install >/dev/null; } || exit 1
# HarfBuzz pela unidade unica (src/harfbuzz.cc): o buster tem meson/cmake velhos
# demais para o 10.x, e assim nao ha libstdc++ dinamica (sem excecao/RTTI/guardas).
HB=/w/src/harfbuzz-10.4.0
if [ ! -f $P/lib/libharfbuzz.a ]; then
  # A unidade unica estoura a memoria do container (cc1plus Killed): cada .cc
  # citado em harfbuzz.cc vira um objeto, dois por vez.
  rm -rf /tmp/hb; mkdir -p /tmp/hb && cd /tmp/hb
  grep -oE '#include "[A-Za-z0-9/-]+\.cc"' $HB/src/harfbuzz.cc | cut -d'"' -f2 > lista
  mkdir -p OT/Var/VARC
  cat lista | xargs -P 2 -I{} g++ $CXXFLAGS -std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden \
    -DHAVE_FREETYPE -DHAVE_PTHREAD -DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR -DNDEBUG -I$HB/src -I$P/include/freetype2 -c $HB/src/{} -o {}.o || exit 1
  ar rcs $P/lib/libharfbuzz.a $(find . -name '*.o')
  mkdir -p $P/include/harfbuzz && cp $HB/src/hb*.h $P/include/harfbuzz/
  rm -f $P/include/harfbuzz/*-private.hh
  cat > $P/lib/pkgconfig/harfbuzz.pc <<PC
prefix=$P
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: harfbuzz
Description: HarfBuzz text shaping library
Version: 10.4.0
Libs: -L\${libdir} -lharfbuzz
Cflags: -I\${includedir}/harfbuzz
PC
fi
cd /w/src/libass-0.17.5
if [ ! -f $P/lib/libass.a ]; then
  make distclean >/dev/null 2>&1 || true
  CPPFLAGS="-I$P/include" LDFLAGS="-L$P/lib" ./configure -q --prefix=$P --enable-static --disable-shared --disable-fontconfig --disable-require-system-font-provider --disable-enca --disable-libunibreak --disable-asm --enable-harfbuzz && make -j6 >/dev/null && make install >/dev/null || exit 1
fi
ls -la $P/lib/*.a
