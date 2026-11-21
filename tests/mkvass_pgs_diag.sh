#!/bin/bash
# Flag de diagnostico do leitor PGS na troca de faixa. Em 03d80de03 o modo
# "corrida" dava rc 66 sob Clang TSan (leitura solta de s->diagLog contra a
# reescrita sob s->trava). O gancho mkvassTesteDiag so existe neste build:
# injetado na copia de mkvass.c gerada abaixo; producao nao o compila.
#   bash tests/mkvass_pgs_diag.sh            (SANITIZE=1 liga ASan+UBSan;
#                                             TSAN=1 liga Clang TSan)
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-diag.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
if [ "${TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
python3 tests/mkvass_pgs_fixture.py "$DIR"
# Copia de mkvass.c com o gancho de escalonamento na entrada de
# publicarDiagnostico — so coordena o interleaving; a sonda usa a API publica.
python3 - "$DIR/mkvass.c" <<'PY'
from pathlib import Path
import sys
src = Path('src/mkvass.c').read_text()
needle = 'static void publicarDiagnostico(Fio *f) {\n'
assert src.count(needle) == 1
hook = needle + '  extern void mkvassTesteDiag(unsigned g);\n  mkvassTesteDiag(f->g);\n'
Path(sys.argv[1]).write_text(src.replace(needle, hook))
PY
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra \
  tests/mkvass_pgs_diag.c "$DIR/mkvass.c" src/{pgs_mkv_restore,imgsub,pgs,legenda,assrender,dados}.c \
  tests/mkvass_i18n.c -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
run() { NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/sparse.mkv" "$1"; }
echo "== diag snapshot (NUVIO_PGS_DIAGNOSTICS=1)"
run diag
echo "== troca de faixa (60x)"
run troca
echo "== corrida (fio estacionado no publicador + selecao)"
run corrida
