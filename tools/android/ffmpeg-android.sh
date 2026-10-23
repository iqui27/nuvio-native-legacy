#!/usr/bin/env bash
# Religa somente o JNI de 64 bits do decoder 1.8.0+1 para paginas de 16 KB.
# Classes Java, codecs e bibliotecas de 32 bits continuam os da publicacao.
# Uso: bash tools/android/ffmpeg-android.sh <aar de saida> [abi...]
set -euo pipefail
cd "$(dirname "$0")/../.."
RAIZ="$PWD"
SAIDA="${1:?passe o caminho do AAR de saida}"; shift
[[ "$SAIDA" = /* ]] || SAIDA="$RAIZ/$SAIDA"
if [ "$#" -gt 0 ]; then ABIS=("$@"); else ABIS=(arm64-v8a armeabi-v7a); fi
case "$(uname -s)" in
  Linux) HOST=linux-x86_64; SDK_DEFAULT="$HOME/Android/Sdk" ;;
  Darwin) HOST=darwin-x86_64; SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
  *) echo "ffmpeg-android.sh: execute no Linux ou macOS com o NDK Android" >&2; exit 1 ;;
esac
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$SDK_DEFAULT}}"
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}"
TC="$NDK/toolchains/llvm/prebuilt/$HOST"
CMAKE_SDK="$SDK/cmake/3.22.1/bin"
if [ -x "$CMAKE_SDK/cmake" ]; then CMAKE="$CMAKE_SDK/cmake"; NINJA="$CMAKE_SDK/ninja"; else CMAKE="$(command -v cmake)"; NINJA="$(command -v ninja)"; fi
[ -f "$NDK/source.properties" ] && [ -x "$TC/bin/clang" ] || { echo "ffmpeg-android.sh: NDK ausente" >&2; exit 1; }
CACHE="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
MEDIA_PIN=b7bbc6e2bc3e45ff3ed99884c114c50f03bba5c9
FFMPEG_PIN=d388c347d41e4eb516dec05910551c5461e65615
AAR_HASH=75404bcf0143c31c88c8904d2384652940af97a0178efd399d7682494615fbff
RECEITA=$( { cksum < "${BASH_SOURCE[0]}"; cksum < "$NDK/source.properties"; } | cksum | awk '{print $1}')
EST="$CACHE/media3-ffmpeg/$RECEITA"
FF="$CACHE/src/ffmpeg-$FFMPEG_PIN"
JNI="$EST/jni"
ORIGINAL="$CACHE/src/media3-ffmpeg-decoder-1.8.0+1.aar"
JOBS="${NUVIO_ANDROID_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
mkdir -p "$CACHE/src" "$JNI" "$(dirname "$SAIDA")"
if [ ! -f "$ORIGINAL" ]; then
  curl -fsSL --retry 3 'https://repo.maven.apache.org/maven2/org/jellyfin/media3/media3-ffmpeg-decoder/1.8.0%2B1/media3-ffmpeg-decoder-1.8.0%2B1.aar' -o "$ORIGINAL.part"
  mv "$ORIGINAL.part" "$ORIGINAL"
fi
python3 - "$ORIGINAL" "$AAR_HASH" <<'PY'
import hashlib, sys
from pathlib import Path
if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() != sys.argv[2]:
    raise SystemExit('ffmpeg-android.sh: AAR publicado nao confere com o SHA256 esperado')
PY
if [ ! -d "$FF" ]; then
  curl -fsSL --retry 3 "https://github.com/FFmpeg/FFmpeg/archive/$FFMPEG_PIN.tar.gz" | tar xz -C "$CACHE/src"
  mv "$CACHE/src/FFmpeg-$FFMPEG_PIN" "$FF"
fi
for f in CMakeLists.txt ffmpeg_jni.cc; do
  if [ ! -s "$JNI/$f" ]; then
    curl -fsSL --retry 3 "https://raw.githubusercontent.com/androidx/media/$MEDIA_PIN/libraries/decoder_ffmpeg/src/main/jni/$f" -o "$JNI/$f.part"
    mv "$JNI/$f.part" "$JNI/$f"
  fi
done
ln -sfn "$FF" "$JNI/ffmpeg"
DECODERS=(flac alac pcm_mulaw pcm_alaw mp3 aac ac3 eac3 dca mlp truehd)
COMUM=(--target-os=android --enable-static --disable-shared --disable-doc
  --disable-programs --disable-everything --disable-avdevice --disable-avformat
  --disable-swscale --disable-postproc --disable-avfilter --disable-symver
  --enable-swresample --extra-ldexeflags=-pie --disable-v4l2-m2m --disable-vulkan
  --enable-pic --disable-autodetect)
for decoder in "${DECODERS[@]}"; do COMUM+=("--enable-decoder=$decoder"); done
AAR_ARGS=("$ORIGINAL" "$SAIDA" "$EST/lib")
for abi in "${ABIS[@]}"; do
  case "$abi" in
    arm64-v8a) ARCH=aarch64; CPU=armv8-a; TRIPLE=aarch64-linux-android21 ;;
    x86_64) ARCH=x86_64; CPU=x86-64; TRIPLE=x86_64-linux-android21 ;;
    armeabi-v7a|x86) continue ;;
    *) echo "ffmpeg-android.sh: ABI desconhecida: $abi" >&2; exit 1 ;;
  esac
  AAR_ARGS+=("$abi")
  FF_OPTS=("${COMUM[@]}")
  if [ "$abi" = x86_64 ]; then FF_OPTS+=(--disable-asm); fi
  LIB="$EST/lib/$abi/libffmpegJNI.so"
  if [ ! -s "$LIB" ]; then
    echo "== FFmpeg JNI 1.8.0+1 $abi, paginas 16 KB"
    mkdir -p "$EST/lib/$abi"
    (
      cd "$FF"
      [ ! -f ffbuild/config.mak ] || make distclean >/dev/null
      ./configure --libdir="android-libs/$abi" --arch="$ARCH" --cpu="$CPU" \
        --cross-prefix="$TC/bin/$TRIPLE-" --cc="$TC/bin/$TRIPLE-clang" --cxx="$TC/bin/$TRIPLE-clang++" \
        --nm="$TC/bin/llvm-nm" --ar="$TC/bin/llvm-ar" \
        --ranlib="$TC/bin/llvm-ranlib" --strip="$TC/bin/llvm-strip" \
        --extra-cflags='-fPIC -D__BIONIC_NO_PAGE_SIZE_MACRO' \
        --extra-ldflags='-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384' \
        "${FF_OPTS[@]}"
      make -j"$JOBS"
      make install-libs
    ) > "$EST/ffmpeg-$abi.log" 2>&1 || { tail -50 "$EST/ffmpeg-$abi.log"; exit 1; }
    B="$EST/build-$abi"
    "$CMAKE" -S "$JNI" -B "$B" -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
      -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-21 -DANDROID_STL=c++_static \
      -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$B" \
      -DCMAKE_SHARED_LINKER_FLAGS='-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384' \
      > "$EST/jni-$abi.log" 2>&1 || { tail -40 "$EST/jni-$abi.log"; exit 1; }
    "$CMAKE" --build "$B" -j"$JOBS" >> "$EST/jni-$abi.log" 2>&1 || { tail -40 "$EST/jni-$abi.log"; exit 1; }
    cp "$B/libffmpegJNI.so" "$LIB"
    "$TC/bin/llvm-strip" --strip-unneeded "$LIB"
  fi
  unzip -p "$ORIGINAL" "jni/$abi/libffmpegJNI.so" > "$EST/lib/$abi/original.so"
  for tipo in original libffmpegJNI; do
    "$TC/bin/llvm-nm" -D --defined-only "$EST/lib/$abi/$tipo.so" | awk '{print $NF}' | grep -E '^(Java_|JNI_OnLoad$|JNI_OnUnload$)' | sort > "$EST/lib/$abi/$tipo.symbols"
  done
  cmp "$EST/lib/$abi/original.symbols" "$EST/lib/$abi/libffmpegJNI.symbols" || { echo "ffmpeg-android.sh: interface JNI mudou: $abi" >&2; exit 1; }
done
# Copia todas as entradas do AAR publicado; troca apenas os JNI pedidos.
python3 - "${AAR_ARGS[@]}" <<'PY'
import importlib.util
from pathlib import Path
import copy, sys, zipfile
spec = importlib.util.spec_from_file_location('pages', 'tools/android-apk-pages.py')
pages = importlib.util.module_from_spec(spec); spec.loader.exec_module(pages)
original, output, libs = map(Path, sys.argv[1:4])
replacements = {}
for abi in sys.argv[4:]:
    data = (libs / abi / 'libffmpegJNI.so').read_bytes()
    _, _, errors = pages.check_elf(data, abi)
    if errors:
        raise SystemExit(f'ffmpeg-android.sh: {abi}: {errors}')
    replacements[f'jni/{abi}/libffmpegJNI.so'] = data
temp = output.with_suffix('.aar.part')
with zipfile.ZipFile(original) as source, zipfile.ZipFile(temp, 'w') as dest:
    names = source.namelist()
    if len(names) != len(set(names)) or not replacements.keys() <= set(names):
        raise SystemExit('ffmpeg-android.sh: entradas do AAR publicado inesperadas')
    for entry in source.infolist():
        data = replacements.get(entry.filename)
        dest.writestr(copy.copy(entry), source.read(entry) if data is None else data)
with zipfile.ZipFile(original) as source, zipfile.ZipFile(temp) as dest:
    for entry in source.infolist():
        if entry.filename not in replacements and source.read(entry) != dest.read(entry.filename):
            raise SystemExit(f'ffmpeg-android.sh: entrada publicada mudou: {entry.filename}')
temp.replace(output)
print(f'ffmpeg-android.sh: {output}, JNI 16 KB: {", ".join(sys.argv[4:]) or "nenhum (32 bits)"}')
PY
