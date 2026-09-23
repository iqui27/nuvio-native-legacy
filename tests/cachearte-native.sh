#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/cachearte.c src/cachearte.c \
  -pthread -o /tmp/nuvio-cachearte-native
/tmp/nuvio-cachearte-native
