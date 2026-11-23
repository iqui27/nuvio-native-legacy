#!/bin/bash
# Fim real durante a reabertura do seek no .tpk (F80, tests/partes_fim_eof_tpk.c):
# o EOS confirmado encerra o "reabrindo" e a parte avanca; a posicao segurada
# nao a prende mais. A parteNoFim e extraida de src/app.c, como em
# tests/partes_fim_seek.sh.
#   SANITIZE=1 ASan+UBSan, SANITIZE=thread TSan.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fimeof.XXXXXX")
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
flags=(-O1 -g -Wall -DNV_TPK)
case "${SANITIZE:-0}" in
  1|asan) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
esac
"${CC:-cc}" "${flags[@]}" -Isrc -I"$work" -I/opt/homebrew/include $(sdl2-config --cflags) \
  tests/partes_fim_eof_tpk.c tests/mkvfix_mp4_stub.c \
  src/video_tpk.c src/video_modo.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c \
  src/audioescolha.c src/mkv.c src/capmkv.c tests/dts_tpk_stub.c -lpthread -lm \
  $(sdl2-config --libs) -o "$work/test"
"$work/test"
