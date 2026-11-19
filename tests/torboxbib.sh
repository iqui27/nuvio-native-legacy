#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-torboxbib.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc tests/torboxbib_mock.c src/torboxbib.c src/js.c \
  -o "$task_dir/teste"
"$task_dir/teste"
