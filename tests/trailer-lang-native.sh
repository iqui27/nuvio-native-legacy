#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #650/#687: trailer language on the native targets (Samsung .tpk / LG ipk).
# On a native build the YouTube id cannot play inside the video plane, but the
# user-language trailer must still be chosen and opened through the external
# launcher (AppControl on tpk, the webOS browser on the ipk). This harness
# compiles the real extras/trailerfonte with NV_TPK to reproduce the host.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -DNUVIO_TRAILER_TEST -DNV_TPK -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections \
  src/extras.c src/notasfontes.c src/trailerfonte.c src/js.c src/rede.c src/redeurl.c tests/trailer-lang-native.c \
  -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread -Wl,-dead_strip \
  -o ${TMPDIR}/nuvio-trailer-lang-native
${TMPDIR}/nuvio-trailer-lang-native
