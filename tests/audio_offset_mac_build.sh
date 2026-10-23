#!/bin/bash
# Isolated output: parallel worktrees must not overwrite each other's nvbuild.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
TMPDIR="$work" bash tools/build-local.sh
file "$work/nvbuild"
