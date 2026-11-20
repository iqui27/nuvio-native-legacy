#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-torboxbib.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc tests/torboxbib.c tests/torboxbib_ui_stubs.c src/torboxbib.c src/torboxbib_ui.c src/js.c \
  -o "$task_dir/teste" -lpthread
"$task_dir/teste"
