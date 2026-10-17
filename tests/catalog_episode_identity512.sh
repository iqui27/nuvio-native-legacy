#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-std=gnu11 -O1 -g -Isrc -pthread -Wall -ffunction-sections -fdata-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
case "$(uname -s)" in Darwin) link=(-Wl,-dead_strip);; *) link=(-Wl,--gc-sections);; esac
cc "${flags[@]}" src/catalogo.c tests/catalog_episode_identity512.c "${link[@]}" \
  -o "${TMPDIR:-/tmp}/nuvio-catalog-episode-identity512"
"${TMPDIR:-/tmp}/nuvio-catalog-episode-identity512"
