#!/bin/bash
# Fim de parte com seek em curso (app.c, parteNoFim): a posicao parada nos
# ultimos 3 s nao pode trocar de arquivo com pedido pendente — seek enviado,
# seek segurado pela sonda, posicao mantida pela reabertura ou a pessoa
# arrastando a barra (tests/partes_fim_seek.c). Compila duas vezes: o ramo
# video_busca_em_voo so existe com -DNV_TPK.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fimseek.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
out = Path(sys.argv[1])
def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
a = Path('src/app.c').read_text()
out.joinpath('fim.c').write_text(function(a, 'static int parteNoFim('))
PY
cc -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function \
  -Isrc -I"$work" tests/partes_fim_seek.c -lm -o "$work/test"
"$work/test"
cc -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function -DNV_TPK \
  -Isrc -I"$work" tests/partes_fim_seek.c -lm -o "$work/test-tpk"
"$work/test-tpk"
