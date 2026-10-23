#!/bin/bash
# arte_url_remota (artefalta.h): funcao pura, sem SDL nem rede.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-artefalta.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc tests/artefalta.c -Isrc -O1 -g -Wall -Wextra -o "$DIR/test"
"$DIR/test"
