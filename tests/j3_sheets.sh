#!/bin/bash
set -uo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-j3-sheets.XXXXXX")
trap 'rm -rf "$work"' EXIT
rc=0
cc tests/botoes_sem_aro.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$work/pills" && "$work/pills" || rc=1
bash tests/aiostreams.sh || rc=1
exit "$rc"
