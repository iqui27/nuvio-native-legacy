#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Build host FFmpeg first with tools/build-dts-ffmpeg.sh NUVIO_DTS_HOST=1.
set -eu
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
  -Wl,--start-group -lavcodec -lavutil -Wl,--end-group -lm -lpthread
"$DTS_SUBTITLE_DIR/subtitles"
