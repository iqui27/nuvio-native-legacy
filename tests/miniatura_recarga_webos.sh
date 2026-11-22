#!/bin/bash
# Recarga webOS no provedor local de miniaturas (F85, tests/miniatura_recarga_webos.c):
# a revisao de carga zera video_pronto()/video_tocando() e o gate do player
# para de publicar — o worker solta o decoder e nao decodifica nada, sem job
# novo e sem chamar video_*/ajustes_* do proprio fio. O gate e a publicacao
# sao extraidos de src/player.c, como em tests/partes_fim_eof_tpk.sh.
#   SANITIZE=1 ASan+UBSan, SANITIZE=thread TSan (clang; o do gcc quebra aqui).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-recarga.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
out = Path(sys.argv[1])
src = Path('src/player.c').read_text()

def stmt(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

gate = stmt(src, 'if (d > 60.0 && video_pronto() && !ehCanal() && ajustes_seekr_ligado()) {')
try:
    pub = stmt(src, 'if (d > 60.0 && !video_pronto() && !ehCanal() && ajustes_seekr_ligado() &&')
except ValueError:
    pub = ''     # pre-F85 trees: nothing outside the gate publishes the drop
out.joinpath('gate.h').write_text(
    'static void batidaPlayer(double d, float posSeg, int scrubbing) {\n'
    + gate + '\n' + pub + '\n}\n')
PY
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Isrc -I"$work" -I/usr/include/SDL2 -DNV_WEBOS -DNV_DTS_FFMPEG)
case "${SANITIZE:-0}" in
  1|asan) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer); CC=clang ;;
esac
"${CC:-cc}" "${flags[@]}" tests/miniatura_recarga_webos.c -lpthread -lm -o "$work/test"
"$work/test"
