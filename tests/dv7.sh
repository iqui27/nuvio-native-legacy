#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
NV_DV7_FIXTURE_OUT="$dir" cargo test --locked --manifest-path tools/dv7/Cargo.toml
NUVIO_DV7_ROOT="$PWD/build/dv7-host" bash tools/build-dv7.sh
cc -std=c11 -Wall -Wextra -Werror -DNV_DV7_CONVERSION \
  -fsanitize=address,undefined -fno-omit-frame-pointer tests/dv7.c \
  build/dv7-host/libnuvio_dv7.a -ldl -lpthread -lm -o "$dir/test"
"$dir/test" "$dir"
