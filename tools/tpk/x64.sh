#!/bin/bash
# Opt-in emulator build: NUVIO_TPK_ARCH=x86_64 bash tools/tpk.sh
# Only NuvioTpk; no ARM hosts, P2P engine or release update artifacts.
set -euo pipefail
cd "$(dirname "$0")/../.." && source tools/ctr.sh
RAIZ="$PWD"
CACHE="${NUVIO_TPK_CACHE:-$HOME/.cache/nuvio-tpk}/x86_64"
SAIDA="$RAIZ/build/tpk-x64"
mkdir -p "$SAIDA" "$CACHE"

ctr_check_system || { echo "Apple Container not running: run 'container system start'" >&2; exit 1; }
ctr_image_exists nuvio-tpk-sdk-x64 ||
  ctr_build --platform linux/amd64 -f tools/tpk/Dockerfile.x64 -t nuvio-tpk-sdk-x64 tools/tpk/

if [ ! -f "$CACHE/prefix/lib/libSDL2.a" ] || [ ! -f "$CACHE/prefix/lib/libSDL2_ttf.a" ] ||
   [ ! -f "$CACHE/prefix/lib/libwebp.a" ] || [ ! -f "$CACHE/prefix/lib/.sdlimage-webp" ] ||
   [ ! -f "$CACHE/prefix/lib/libass.a" ] || [ ! -f "$CACHE/prefix/lib/.ttf-freetype-externo" ]; then
  echo "[deps] x86_64 static dependencies (first build takes a while)"
  mkdir -p "$CACHE/src"
  for u in https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz \
           https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz \
           https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz \
           https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz \
           https://download-mirror.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz \
           https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz \
           https://github.com/harfbuzz/harfbuzz/releases/download/10.4.0/harfbuzz-10.4.0.tar.xz \
           https://github.com/libass/libass/releases/download/0.17.5/libass-0.17.5.tar.xz; do
    d="$(basename "$u")"; d="${d%.tar.*}"
    [ -d "$CACHE/src/$d" ] || { curl -fsSL "$u" | tar x -C "$CACHE/src"; }
  done
  cp tools/tpk/deps.sh "$CACHE/deps.sh"
  ctr_run --rm --platform linux/amd64 -v "$CACHE:/w" nuvio-tpk-sdk-x64 sh /w/deps.sh
fi

echo "[1/3] libnuvio.so (x86_64, glibc <= 2.28)"
ENVF=$(mktemp "${TMPDIR:-/tmp}/nuvio-tpk-x64-env.XXXXXXXX")
trap 'rm -f "$ENVF"' EXIT
tools/env.sh --env-file "$ENVF"
ctr_run --rm --platform linux/amd64 --env-file "$ENVF" \
  -e NUVIO_BUILD_JOBS="${NUVIO_BUILD_JOBS:-6}" \
  -e NUVIO_SDK_ID="$(ctr_image_inspect nuvio-tpk-sdk-x64):$CACHE" \
  -e NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-}" -e NV_TPK_SEM_ASS="${NV_TPK_SEM_ASS:-}" \
  -v "$RAIZ:/work" -v "$CACHE/prefix:/deps" -w /work nuvio-tpk-sdk-x64 sh -c '
  set -e
  ASS_CFLAGS="-DNV_ASS_LIBASS"; ASS_LIBS="-lass -lharfbuzz -lfribidi -lfreetype"
  if [ "${NV_TPK_SEM_ASS:-}" = 1 ]; then ASS_CFLAGS=""; ASS_LIBS="-lfreetype"; fi
  for k in NV_SUPABASE_URL NV_SUPABASE_ANON_KEY NV_TV_LOGIN_BASE NV_TRAKT_CLIENT_ID \
           NV_TRAKT_CLIENT_SECRET NV_SIMKL_CLIENT_ID NV_SIMKL_APP NV_TMDB_API_KEY NV_SEEKR_API_KEY \
           NV_REC_URL NV_DISCORD_CLIENT_ID NV_VERSAO; do
    eval "v=\${$k:-}"
    v=$(printf "%s" "$v" | sed "s/[\\\\\"]/\\\\&/g")
    printf "%s\n" "-D$k=\\\"$v\\\""
  done > /tmp/flags
  FLAGS="$CFLAGS -DNV_TPK $ASS_CFLAGS -include src/tpk.h -fvisibility=hidden -Wno-unused-result $NUVIO_EXTRA_CFLAGS"
  KEY=$( { cat /tmp/flags; sha256sum tools/tpk/x64.sh tools/native-objects.mk; printf "%s" "$NUVIO_SDK_ID|$FLAGS"; } | sha256sum | cut -d" " -f1)
  O="build/tpk-x64/objects/$KEY"
  SOURCES=$(ls src/*.c src/dts/*.c | grep -v "src/video_tizen.c")
  make -s -f tools/native-objects.mk -j "$NUVIO_BUILD_JOBS" CC=gcc OBJDIR="$O" SOURCES="$SOURCES" \
    NATIVE_FLAGS="$FLAGS @/tmp/flags -I/deps/include -I/deps/include/SDL2"
  ORDER=$(for f in $SOURCES; do echo "$(basename "$f") $f"; done | sort | cut -d" " -f2)
  OBJECTS=""
  for f in $ORDER; do OBJECTS="$OBJECTS $O/${f%.c}.o"; done
  gcc -shared -o build/tpk-x64/libnuvio.so $OBJECTS -Wl,--no-undefined \
    -Wl,-soname,libnuvio.so -Wl,--exclude-libs,ALL \
    -L/deps/lib -lSDL2_ttf -lSDL2_image -lwebpdemux -lwebp -lsharpyuv -lSDL2 $ASS_LIBS \
    /usr/lib/x86_64-linux-gnu/libz.a -lGLESv2 -ldl -lpthread -lm -lrt
  file build/tpk-x64/libnuvio.so
  objdump -T build/tpk-x64/libnuvio.so | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -1
  objdump -p build/tpk-x64/libnuvio.so | grep NEEDED
  for f in $(grep -rhoE "DllImport\(\"libnuvio.so\"\)\] static extern [a-z]+ nv_[a-z0-9_]+" tizen-tpk/*.cs | awk "{print \$NF}" | sort -u); do
    objdump -T build/tpk-x64/libnuvio.so | grep -qE " $f\$" || { echo "libnuvio.so missing export $f" >&2; exit 1; }
  done
'

echo "[2/3] NuvioTpk .NET host + emulator package"
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1
VER=$(sed -n 's/^NV_VERSAO=//p' "$ENVF")
ARTE=$(NUVIO_ARTE_ESTAGIO="$SAIDA/art-pacote" bash tools/tizen-art.sh)
# Isolate native libraries, resources AND .NET outputs from the ARM host.
rm -rf "$SAIDA/host"
H="$SAIDA/host/NuvioTpk"
mkdir -p "$H/lib" "$H/res" "$H/shared/res"
cp tizen-tpk/*.cs "$SAIDA/host/"
cp tizen-tpk/NuvioTpk/NuvioTpk.csproj "$H/"
cp "$SAIDA/libnuvio.so" "$H/lib/"
cp -R "$ARTE" "$H/res/art"
printf '%s' "$VER" > "$H/res/versao.txt"
cp -R deploy/app/fonts deploy/app/licencas "$H/res/"
cp tizen-tpk/silencio.mp4 "$H/res/"
cp deploy/app/tizen/icon.png "$H/shared/res/NuvioTpk.png"
# Samsung.NET.Sdk preserves lib/ for single-architecture packages; Program.cs
# explicitly dlopens lib/libnuvio.so. No lib64/ or manifest arch override needed.
sed "s/ version=\"[^\"]*\">/ version=\"$VER\">/" tizen-tpk/NuvioTpk/tizen-manifest.xml > "$H/tizen-manifest.xml"
dotnet build "$H/NuvioTpk.csproj" -c Release -nologo -v q
TPK=$(find "$H/bin/Release" -name '*.tpk' | head -1)
[ -n "$TPK" ] || { echo "NuvioTpk: dotnet did not generate a .tpk" >&2; exit 1; }

echo "[3/3] verifying package"
L=$(unzip -Z1 "$TPK")
if grep -E '(^|/)(conta-[^/]*|discord-p[^/]*|jellyfin-p[^/]*|emby-p[^/]*|plex-p[^/]*)\.txt(\.tmp)?$' <<<"$L"; then
  echo "Package contains private account files" >&2; exit 1
fi
for f in lib/libnuvio.so bin/NuvioTpk.dll res/versao.txt res/silencio.mp4 res/licencas/p2p-avisos.txt \
         res/art/marcas/abertura.jpg res/art/marcas/login-fundo.jpg \
         res/art/marcas/logo-novo-marca.png res/art/marcas/logo-classico.png; do
  grep -qxF "$f" <<<"$L" || { echo "Package missing $f" >&2; exit 1; }
done
unzip -p "$TPK" lib/libnuvio.so > "$SAIDA/libnuvio-packaged.so"
cmp "$SAIDA/libnuvio.so" "$SAIDA/libnuvio-packaged.so"
file "$SAIDA/libnuvio-packaged.so" | grep 'ELF 64-bit.*x86-64'
DESTINO="$SAIDA/Nuvio-$VER-NuvioTpk-x64.tpk"
cp "$TPK" "$DESTINO"
echo "  $DESTINO ($(du -h "$DESTINO" | cut -f1))"
echo 'Emulator only, no P2P engine. Re-sign with the Samsung dev profile on the Windows PC before installing.'
