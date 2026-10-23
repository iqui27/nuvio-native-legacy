#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Escala do retangulo de video (layout 1920x1080 -> drawable), issue #176.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra tests/video_escala.c -o ${TMPDIR:-/tmp}/nuvio-video-escala-tests
${TMPDIR:-/tmp}/nuvio-video-escala-tests
