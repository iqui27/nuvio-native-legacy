#!/bin/bash
# The TVDB-to-absolute table (metaep_tvdb_*) against the per-episode scan it
# replaced in the Simkl anime import: same answer for every question.
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-metaep-tvdb-XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
cc -O1 -g -Wall -Wextra -Werror -Isrc src/metaep.c src/js.c tests/metaep_tvdb.c \
  -o "$task_tmp/test" -lpthread -lm
"$task_tmp/test"
