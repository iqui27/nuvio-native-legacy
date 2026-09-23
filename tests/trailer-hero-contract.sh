#!/bin/bash
# Contrato do trailer na Samsung: executa os corpos EM_JS reais de src/trailer.c.
set -eu
cd "$(dirname "$0")/.."
node tests/trailer-hero-contract.cjs
