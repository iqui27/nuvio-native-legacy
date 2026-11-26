#!/usr/bin/env bash
# FFmpeg for the seek-bar thumbnails made on the device (src/miniatura*.c) on
# Android: static libavformat/libavcodec/libavutil per ABI, with ONLY what the
# thumbnail engine uses (the .tpk and LG get the same set inside their DTS
# FFmpeg, tools/build-dts-ffmpeg.sh):
#   decoders h264, hevc (keyframes only) - encoder mjpeg (320x180 tiles)
#   demuxers matroska, mov, mpegts - parsers h264, hevc - zlib (NDK's libz)
# Media3's FFmpeg AAR (tools/android/ffmpeg-android.sh) is a separate audio
# decoder for the player and stays as it is.
#
# Usage: bash tools/android/ffmpeg-miniaturas.sh <output dir> [abi...]
# Output: <dir>/<abi>/{include,lib} (CMake: -DNUVIO_MINI_FFMPEG=<dir>).
# Rebuilds only when this script, the FFmpeg pin or the NDK change.
set -euo pipefail
cd "$(dirname "$0")/../.."
RAIZ="$PWD"
SAIDA="${1:?pass the output directory}"; shift
[[ "$SAIDA" = /* ]] || SAIDA="$RAIZ/$SAIDA"
if [ "$#" -gt 0 ]; then ABIS=("$@"); else ABIS=(arm64-v8a armeabi-v7a); fi
case "$(uname -s)" in
  Linux) HOST=linux-x86_64; SDK_DEFAULT="$HOME/Android/Sdk" ;;
  Darwin) HOST=darwin-x86_64; SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
  *) echo "ffmpeg-miniaturas.sh: run on Linux or macOS with the Android NDK" >&2; exit 1 ;;
esac
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$SDK_DEFAULT}}"
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}"
TC="$NDK/toolchains/llvm/prebuilt/$HOST"
[ -f "$NDK/source.properties" ] && [ -x "$TC/bin/clang" ] || { echo "ffmpeg-miniaturas.sh: NDK missing ($NDK)" >&2; exit 1; }
# Same pinned release as the Samsung/LG FFmpeg.
VERSION=$(sed -n 's/^VERSION=//p' tools/build-dts-ffmpeg.sh | head -1)
SHA256=$(sed -n 's/^SHA256=//p' tools/build-dts-ffmpeg.sh | head -1)
CACHE="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
RECEITA=$( { cksum < "${BASH_SOURCE[0]}"; cksum < "$NDK/source.properties"; printf '%s' "$VERSION"; } | cksum | awk '{print $1}')
JOBS="${NUVIO_ANDROID_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
ARQ="$CACHE/src/ffmpeg-$VERSION.tar.xz"
mkdir -p "$CACHE/src" "$SAIDA"
if [ ! -f "$ARQ" ]; then
  curl -fsSL --retry 3 "https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz" -o "$ARQ.part"
  mv "$ARQ.part" "$ARQ"
fi
python3 - "$ARQ" "$SHA256" <<'PY'
import hashlib, sys
from pathlib import Path
if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() != sys.argv[2]:
    sys.exit("ffmpeg-miniaturas.sh: FFmpeg tarball SHA-256 mismatch")
PY

for abi in "${ABIS[@]}"; do
  DEST="$SAIDA/$abi"
  if [ -f "$DEST/.receita" ] && [ "$(cat "$DEST/.receita")" = "$RECEITA" ]; then
    echo "  FFmpeg (thumbnails) $abi: cached"; continue
  fi
  case "$abi" in
    arm64-v8a)   ARCH=aarch64; CPU=armv8-a; CC_ABI=aarch64-linux-android24; EXTRA=() ;;
    armeabi-v7a) ARCH=arm; CPU=armv7-a; CC_ABI=armv7a-linux-androideabi24; EXTRA=(--enable-neon) ;;
    *) echo "ffmpeg-miniaturas.sh: unsupported ABI $abi" >&2; exit 1 ;;
  esac
  OBRA="$CACHE/mini-ffmpeg-build/$abi"
  rm -rf "$OBRA" "$DEST"; mkdir -p "$OBRA"
  tar -xf "$ARQ" -C "$OBRA"
  ( cd "$OBRA/ffmpeg-$VERSION"
    ./configure --prefix="$DEST" --enable-cross-compile --target-os=android \
      --arch="$ARCH" --cpu="$CPU" "${EXTRA[@]}" \
      --cc="$TC/bin/$CC_ABI-clang" --cxx="$TC/bin/$CC_ABI-clang++" \
      --ar="$TC/bin/llvm-ar" --ranlib="$TC/bin/llvm-ranlib" --nm="$TC/bin/llvm-nm" \
      --strip="$TC/bin/llvm-strip" \
      --disable-everything --disable-autodetect --enable-zlib \
      --disable-shared --enable-static --enable-pic \
      --disable-programs --disable-doc --disable-debug --disable-network \
      --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample --disable-postproc \
      --enable-avcodec --enable-avformat --enable-avutil \
      --enable-decoder=h264,hevc --enable-encoder=mjpeg \
      --enable-demuxer=matroska,mov,mpegts --enable-parser=h264,hevc \
      --extra-cflags="-O2 -fvisibility=hidden"
    make -j"$JOBS"
    make install )
  cp "$OBRA/ffmpeg-$VERSION/COPYING.LGPLv2.1" "$DEST/"
  printf 'FFmpeg %s\nhttps://ffmpeg.org/releases/ffmpeg-%s.tar.xz\nSHA256 %s\n' "$VERSION" "$VERSION" "$SHA256" > "$DEST/SOURCE.txt"
  printf '%s' "$RECEITA" > "$DEST/.receita"
  rm -rf "$OBRA"
  echo "  FFmpeg (thumbnails) $abi: $(du -sh "$DEST/lib" | cut -f1)"
done
