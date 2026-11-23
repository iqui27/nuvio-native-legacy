#!/bin/bash
# Contrato aparelho.modelo x servidor publicado (farol-8, volta 3): compila a
# sonda C (coletor + redator de producao, caminho do receptor webOS) e roda o
# .mjs que valida cada modelo emitido pelo modeloOk REAL de src/farol.js e o
# registro inteiro pela rotaFarolRegistro local. Precisa de node 22+
# (node:sqlite); sem node, pula.
#   bash tests/farol_modelo_contrato.sh
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "farol_modelo_contrato: sem node, pulado"; exit 0; }
base_tmp="${TMPDIR:-/tmp}"
base_tmp="${base_tmp%/}"
bin=$(mktemp "${base_tmp}/nuvio-farol-modelo-contrato.XXXXXX")
trap 'rm -f "$bin"' EXIT
"${CC:-cc}" -Wall -Wextra -O1 -g -pthread -DWEBOS \
  tests/farol_modelo_contrato.c src/farolperfil.c src/farolredator.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -o "$bin"
env -i LC_ALL=C TMPDIR="$base_tmp" PATH="$PATH" node tests/farol_modelo_contrato.mjs "$bin"
