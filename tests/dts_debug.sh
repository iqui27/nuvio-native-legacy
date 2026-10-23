#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
TMP=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-dts-debug-test.XXXXXXXX)
trap 'rm -rf "$TMP"' EXIT
for mode in production debug; do
  FLAGS=''
  if [ "$mode" = debug ]; then FLAGS='-DNV_DTS_DEBUG -DNV_APP_ID="space.nuvio.native.legacy.dtsdebug"'; fi
  ${CC:-cc} -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Isrc \
    -DNV_WEBOS -DNV_DTS_FFMPEG $FLAGS tests/dts_debug_identity.c src/dts/dts_playback.c \
    -Wl,--gc-sections -pthread -lm -o "$TMP/identity"
  "$TMP/identity"
  ${CC:-cc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -Isrc $FLAGS \
    tests/dts_debug_data.c src/dados.c -pthread -o "$TMP/data"
  mkdir "$TMP/$mode"
  "$TMP/data" "$TMP/$mode" env
  "$TMP/data" "$TMP/$mode" home
  printf 'DTS %s identity, feature gate and isolated data passed\n' "$mode"
done
# The default feature still requires both the webOS target and its decoder.
for FLAGS in '' '-DNV_WEBOS' '-DNV_DTS_FFMPEG'; do
  ${CC:-cc} -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Isrc \
    $FLAGS tests/dts_debug_identity.c src/dts/dts_playback.c \
    -Wl,--gc-sections -pthread -lm -o "$TMP/platform"
  "$TMP/platform"
done
printf 'DTS unavailable without webOS or FFmpeg, including legacy launch variables\n'
${CC:-cc} -std=gnu11 -ffunction-sections -fdata-sections -Isrc \
  -DNV_DTS_DEBUG '-DNV_APP_ID="space.nuvio.native.legacy.dtsdebug"' \
  tests/dts_debug_updater.c -Wl,--gc-sections -lm -o "$TMP/updater"
"$TMP/updater"
# A debug binary cannot accidentally inherit the default production identity.
if ${CC:-cc} -Isrc -DNV_DTS_DEBUG -x c -c -o "$TMP/invalid.o" - <<'C' 2>"$TMP/expected-error"
#include "app_id.h"
C
then echo 'Debug app without explicit identity unexpectedly compiled' >&2; exit 1; fi
printf 'DTS debug production updater disabled and explicit identity enforced\n'
