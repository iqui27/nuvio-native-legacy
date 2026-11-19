#!/bin/bash
# Compile/link the webOS GLES2 target offline. No arm.sh, packaging or device.
set -eu
cd "$(dirname "$0")/.." && source tools/ctr.sh
ctr_run --rm --network none -v "$PWD:/work:ro" -w /work \
  nuvio-webos-sdk sh -ec '
  sr=$NUVIO_SYSROOT
  arm-webos-linux-gnueabi-gcc -c src/p2pmotor.c -o /tmp/p2pmotor.o \
    -O1 -DNV_WEBOS -D_FILE_OFFSET_BITS=64 -I"$sr/usr/include" -I"$sr/usr/include/SDL2"
  sources=""
  for source in src/*.c src/dts/*.c; do
    [ "$source" = src/p2pmotor.c ] || sources="$sources $source"
  done
  arm-webos-linux-gnueabi-gcc $sources /tmp/p2pmotor.o -o /tmp/nuvio-vidro-c9.arm \
    -O1 -DNV_WEBOS \
    -DNV_SUPABASE_URL=\"\" -DNV_SUPABASE_ANON_KEY=\"\" -DNV_TV_LOGIN_BASE=\"\" \
    -DNV_TRAKT_CLIENT_ID=\"\" -DNV_TRAKT_CLIENT_SECRET=\"\" \
    -DNV_SIMKL_CLIENT_ID=\"\" -DNV_SIMKL_APP=\"\" -DNV_TMDB_API_KEY=\"\" \
    -I"$sr/usr/include" -I"$sr/usr/include/SDL2" \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lz -lm
  file /tmp/nuvio-vidro-c9.arm
'
