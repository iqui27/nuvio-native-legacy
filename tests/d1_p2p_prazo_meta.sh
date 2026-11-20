#!/bin/bash
# #775 directed test (Linux/VPS).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-d1-p2p.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -Wall -Wextra -Isrc tests/d1_linux_ajustes_shim.c tests/d1_p2p_prazo_meta.c \
  -o "$work/test" -lpthread
NUVIO_DADOS="$work" "$work/test"
printf 'd1_p2p_prazo_meta.sh: ok\n'
