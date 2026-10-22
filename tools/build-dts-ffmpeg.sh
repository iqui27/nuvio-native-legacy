#!/bin/sh
# Minimal static LGPL FFmpeg: DTS decode, stereo AAC and AC3 5.1 encode (no GPL/nonfree). No FFmpeg
# network/TLS protocols or software video decoders enter the TV executable.
set -eu
VERSION=7.1.5
SHA256=de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f
PREFIX=${NUVIO_DTS_ROOT:-/opt/nuvio-dts}
BUILD=${NUVIO_DTS_BUILD:-/tmp/nuvio-dts-build}
CC=${CC:-arm-webos-linux-gnueabi-gcc}
mkdir -p "$BUILD" "$PREFIX"
archive="$BUILD/ffmpeg-$VERSION.tar.xz"
[ -f "$archive" ] || curl -fsSL "https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz" -o "$archive"
printf '%s  %s\n' "$SHA256" "$archive" | sha256sum -c -
tar -xf "$archive" -C "$BUILD"
cd "$BUILD/ffmpeg-$VERSION"
# NUVIO_DTS_HOST=1 builds the identical components for fixture tests.
if [ "${NUVIO_DTS_HOST:-0}" = 1 ]; then
  set -- --cc="$CC" --disable-x86asm
else
  set -- --enable-cross-compile --arch=arm --target-os=linux \
    --cross-prefix="${CC%-gcc}-" --cc="$CC" --sysroot="${NUVIO_SYSROOT:?SDK sysroot required}"
fi
./configure "$@" --prefix="$PREFIX" --disable-everything \
  --disable-autodetect --disable-shared --enable-static --enable-pic \
  --disable-programs --disable-doc --disable-debug --disable-network \
  --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample \
  --enable-decoder=dca,dvdsub,pgssub,ass,ssa,subrip,movtext,webvtt --enable-encoder=aac,ac3 \
  --enable-demuxer=matroska,mov --enable-parser=dca,h264,hevc,aac \
  --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,dca_core \
  --extra-cflags="${CFLAGS:--O2}"
make -j"${NUVIO_DTS_JOBS:-2}"
make install
# Include license text and pin provenance for redistribution of static LGPL libs.
cp COPYING.LGPLv2.1 "$PREFIX/"
printf 'FFmpeg %s\nhttps://ffmpeg.org/releases/ffmpeg-%s.tar.xz\nSHA256 %s\n' "$VERSION" "$VERSION" "$SHA256" > "$PREFIX/SOURCE.txt"
