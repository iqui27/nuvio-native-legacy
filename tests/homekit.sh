#!/bin/bash
# Kit de Home importavel por URL (homekit.c): leitura, recusas, tetos e a ida e
# volta do JSON guardado. So a regra pura e js.c; sem SDL, sem rede, sem disco.
#   bash tests/homekit.sh
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-homekit"
cc src/homekit.c src/js.c tests/homekit.c -Isrc -o "$out" \
  -O1 -g -Wall -Wextra -fsanitize=address,undefined
"$out"
rm -f "$out"
# O download do kit pela reserva do Android respeita o teto (rede_android_teto).
SANITIZE=1 bash tests/rede_android_teto.sh
