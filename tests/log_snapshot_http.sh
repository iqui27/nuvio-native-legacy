#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d "${TMPDIR:-/tmp}/snapshot-http.XXXXXX"); trap 'rm -rf "$t"' EXIT
# An optional extracted source tree is used only for baseline evidence.
source_root=${1:-.}
flags=()
if ! rg -q 'celular_abrir_relatorio' "$source_root/src/celular.h"; then flags+=(-DPRE_PORT_SERVER); fi
cc -std=gnu11 "${flags[@]}" -Isrc -I"$source_root/src" "$source_root/src/celular.c" tests/log_snapshot_http.c -o "$t/test" -lpthread
"$t/test"
