#!/bin/bash
# Farol (issue 35): faroltexto_aceita() aceita so frase no tom da casa —
# roda o modulo puro sobre 74 boas e 99 ruins (largura, 3a linha, UTF-8,
# controle, URL, emoji e a lista "nunca diz" do ALMA 2).
set -eu
cd "$(dirname "$0")/.."
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-aceita.XXXXXX")"
bin="$dir/t"
trap 'rm -rf "$dir"' EXIT
cc -Wall -Wextra -O1 -g tests/farol_aceita.c src/faroltexto.c -Isrc \
   -I/opt/homebrew/include -o "$bin"
"$bin"
echo "farol_aceita: tudo ok"
