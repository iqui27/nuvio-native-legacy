#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -DNUVIO_TRAILER_TEST -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections \
  src/extras.c src/js.c src/rede.c src/redeurl.c tests/extras-hero.c \
  -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread -Wl,-dead_strip \
  -o /tmp/nuvio-extras-hero-tests
/tmp/nuvio-extras-hero-tests
