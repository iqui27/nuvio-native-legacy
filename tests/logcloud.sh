#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Isrc -DNV_LOGCLOUD_SEM_REDE src/logcloud.c tests/logcloud.c -o "$t/test"
"$t/test"
