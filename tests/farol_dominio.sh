#!/bin/bash
# Farol (issue 35): dominio sem esquema nao entra no texto do gosto —
# separador tokeniza palavra, nao apaga o rotulo antes do ponto. Roda a
# producao sobre os 364 rotulos da varredura IDN da revisao + os casos
# nomeados, e mantem os controles aceitos.
set -eu
cd "$(dirname "$0")/.."
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-dominio.XXXXXX")"
bin="$dir/t"
trap 'rm -rf "$dir"' EXIT
cc -Wall -Wextra -O1 -g tests/farol_dominio.c src/faroltexto.c -Isrc \
   -I/opt/homebrew/include -o "$bin"
"$bin"
echo "farol_dominio: tudo ok"
