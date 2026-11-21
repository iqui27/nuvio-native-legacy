#!/bin/bash
# Farol (issue 35): faroltexto_aceita() aceita so frase no tom da casa —
# roda o modulo puro sobre 60 boas e 60 ruins (largura, 3a linha, UTF-8,
# controle, URL, emoji e a lista "nunca diz" do ALMA 2).
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O1 -g tests/farol_aceita.c src/faroltexto.c -Isrc \
   -I/opt/homebrew/include -o /tmp/nuvio-farol-aceita
/tmp/nuvio-farol-aceita
rm -f /tmp/nuvio-farol-aceita
echo "farol_aceita: tudo ok"
