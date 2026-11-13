#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simklanime-XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
cc -O1 -g -Wall -Wextra -Werror -Isrc src/simklanime.c tests/simklanime.c \
  -o "$task_tmp/test" -lpthread
"$task_tmp/test"
