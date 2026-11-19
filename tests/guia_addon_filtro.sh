#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-addon.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
cc -O1 -g -Wall -Wextra tests/guia_addon_filtro.c -o "$task_dir/teste"
"$task_dir/teste"
