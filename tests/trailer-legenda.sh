#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
node tests/trailer-legenda.cjs
