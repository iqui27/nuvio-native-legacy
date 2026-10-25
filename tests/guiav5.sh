#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiav5.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/guiav5.c src/guiav5.c src/js.c -o "$work/test"
"$work/test"
cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/guiav5_episodes.c src/guiav5.c src/js.c -o "$work/episodes"
"$work/episodes"
cc -std=gnu11 -Isrc -Itests src/celular.c tests/guiav5_phone.c -lpthread -o "$work/phone"
"$work/phone"
bash tests/guia_hold.sh
