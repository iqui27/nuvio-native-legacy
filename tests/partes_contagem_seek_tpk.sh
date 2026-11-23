#!/bin/bash
# Contagem de fim de parte durante a reabertura do seek no .tpk (F80,
# tests/partes_contagem_seek_tpk.c): o vencimento sozinho espera a busca
# assentar; o "Assistir agora" da pessoa troca na hora. Backend de producao
# com host/relogio simulados; o bloco de avanco do app.c (da marca de
# creditos a parteTrocarPara) e a parteNoFim sao extraidos aqui, como em
# tests/partes_fim_seek.sh.
#   SANITIZE=1 ASan+UBSan, SANITIZE=thread TSan.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-contagem.XXXXXX")
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
start = a.index("    { double cred = parteCreditos(d);")
end = a.index("\n  }\n}", start)
avanco = ("static void frame(double d, double pos) {\n"
          "  const PartesLista *sl = &lista;\n"
          "  const int k = 0, prox = 1;\n"
          "  const char *alvo = \"fixture\";\n"
          + a[start:end] + "\n}\n")
out.joinpath('avanco.c').write_text(avanco)
PY
flags=(-O1 -g -Wall -DNV_TPK -std=gnu11 -ffunction-sections -fdata-sections)
case "${SANITIZE:-0}" in
  1|asan) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
esac
"${CC:-cc}" "${flags[@]}" -Isrc -I"$work" -I/opt/homebrew/include $(sdl2-config --cflags) \
  tests/partes_contagem_seek_tpk.c src/partescartao.c tests/mkvfix_mp4_stub.c \
  src/video_tpk.c src/video_modo.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c \
  src/audioescolha.c src/mkv.c src/capmkv.c tests/dts_tpk_stub.c -lpthread -lm \
  -Wl,--gc-sections $(sdl2-config --libs) -o "$work/test"
"$work/test"
