#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-canal-tpk-eof.XXXXXX)
trap 'rm -rf "$work"' EXIT
cc -Wall -Wextra -Werror -DNV_TPK tests/canal_tpk_eof.c -o "$work/tpk"
"$work/tpk"
cc -Wall -Wextra -Werror tests/canal_tpk_eof.c -o "$work/outros"
"$work/outros"
