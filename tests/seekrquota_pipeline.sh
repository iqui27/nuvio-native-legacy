#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-seekrquota-pipeline.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/seekrquota.c src/dados.c src/seekrvtt.c src/js.c src/jpegrapido.c \
  tests/seekrquota_pipeline.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -framework OpenGL -pthread -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  -o ${TMPDIR:-/tmp}/nuvio-seekrquota-pipeline
NUVIO_DADOS="$quota_dir" ${TMPDIR:-/tmp}/nuvio-seekrquota-pipeline
