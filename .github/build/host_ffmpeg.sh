#!/usr/bin/env bash
# Same minimal components as tools/build-dts-ffmpeg.sh. The official Git tag's
# commit is pinned so host tests do not depend on ffmpeg.org archive uptime.
# The production ARM SDK continues to use its existing verified build script.
set -euo pipefail
prefix=${NUVIO_DTS_ROOT:?}
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
git clone --quiet --depth=1 --branch n7.1.5 --single-branch https://github.com/FFmpeg/FFmpeg.git "$build/source"
test "$(git -C "$build/source" rev-parse HEAD)" = 3a0867c2bfda4a4d4309ca1a8cbdc6175e67f587
cd "$build/source"
./configure --cc=gcc --disable-x86asm --prefix="$prefix" \
  --disable-everything --disable-autodetect --disable-shared --enable-static --enable-pic \
  --disable-programs --disable-doc --disable-debug --disable-network \
  --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample \
  --enable-decoder=dca,truehd,ac3,eac3,dvdsub,pgssub,ass,ssa,subrip,movtext,webvtt \
  --enable-encoder=aac,ac3 --enable-demuxer=matroska,mov --enable-parser=dca,h264,hevc,aac,mlp,ac3 \
  --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,dca_core --extra-cflags=-O2
make -j"${NUVIO_DTS_JOBS:-3}"
make install
