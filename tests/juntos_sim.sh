#!/bin/bash
# Watch Together: simulador de pares (4 motores, rede ruim, players A/B).
#   bash tests/juntos_sim.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntossim.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc src/juntossync.c src/juntosproto.c src/juntosmotor.c src/js.c \
  tests/juntos_sim.c -lm -o "$work/t"
"$work/t"
