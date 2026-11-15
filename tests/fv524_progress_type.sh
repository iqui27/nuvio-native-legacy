#!/bin/bash
# #524: opposite-type shared-base-id progress application at cat_definir_tudo boundary.
# UNEXECUTED characterization — central has NOT authorized compile/link/run for this lease.
# Non-executable reference recipe: always exits 2. Activation needs a separately
# authorized edit that removes the guard below and uncomments the build lines.
set -eu
cd "$(dirname "$0")/.."
echo "fv524_progress_type: NOTRUN (compile/runtime blocked for this lease)" >&2
exit 2
# --- reference link line (do not execute under lease) ---
# TMPDIR="${TMPDIR:-/tmp}"
# flags=()
# if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/fv524_progress_type.c \
#   -Isrc -o "${TMPDIR}/nuvio-fv524-progress-type" -O1 -g \
#   -Wall -Wno-deprecated-declarations -Wno-macro-redefined
# "${TMPDIR}/nuvio-fv524-progress-type"
