#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Build host FFmpeg first with tools/build-dts-ffmpeg.sh NUVIO_DTS_HOST=1.
set -eu
# FFmpeg group link: GNU ld needs --start-group for circular static refs;
# Apple ld only lists archives once (all dead-strippable symbols resolved).
if [[ $(uname -s) == Darwin ]]; then
  AV_LIBS="-lavformat -lavcodec -lswresample -lavutil -lz"
else
  AV_LIBS="-Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lz"
fi
cd "$(dirname "$0")/.."
DTS_SUBTITLE_PREFIX=${NUVIO_DTS_ROOT:-${TMPDIR:-/tmp}/nuvio-dts-host}
DTS_SUBTITLE_DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-dts-subtitles.XXXXXXXX)
trap 'rm -rf "$DTS_SUBTITLE_DIR"' EXIT
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Isrc \
  src/dts/dts_subtitles.c tests/dts_subtitles_stub.c -o "$DTS_SUBTITLE_DIR/stub"
"$DTS_SUBTITLE_DIR/stub"
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG \
  -I"$DTS_SUBTITLE_PREFIX/include" -Isrc tests/dts_subtitles.c \
  -o "$DTS_SUBTITLE_DIR/subtitles" -L"$DTS_SUBTITLE_PREFIX/lib" \
  $AV_LIBS -lm -lpthread
"$DTS_SUBTITLE_DIR/subtitles"
