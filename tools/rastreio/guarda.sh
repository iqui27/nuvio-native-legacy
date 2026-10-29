#!/bin/bash
# Guarda de regressao. Ver tools/rastreio/README.md.
cd "$(dirname "$0")/../.." || exit 1
exec python3 tools/rastreio/guarda.py "$@"
