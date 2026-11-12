#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
node --test servidor/recomendacoes/teste-loglink.mjs
