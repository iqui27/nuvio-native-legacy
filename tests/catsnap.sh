#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-catsnap.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/catsnap.c -o "$work/test"
"$work/test"
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Isrc tests/catsnap_disk.c -o "$work/disk"
"$work/disk"
