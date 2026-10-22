#!/bin/bash
# #293: tabela de nomes/canais do codec de audio.
set -eu
cd "$(dirname "$0")/.."
T=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-audioinfo.XXXXXX"); trap 'rm -rf "$T"' EXIT
cc -Isrc -Wall -Wextra tests/audioinfo.c src/audioinfo.c -o "$T/t"
"$T/t"
