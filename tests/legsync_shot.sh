#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da linha de AutoSync na folha de legendas. Nao entra na suite
# (*_shot.sh): janela GL. HTTP real contra servidor Range local.
#   bash tests/legsync_shot.sh /Volumes/ExternalSSD/nv-f05-shots/legsync
set -eu
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}; FX="$T/nv-legref-fx"
bash tests/legref_fixtures.sh "$FX"
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
# LS_RITMO=0: sem o teto de 8 Ranges/s (servidor local).
cc "${sources[@]}" tests/legsync_shot.c -Isrc -DLS_RITMO=0 -o ${TMPDIR:-/tmp}/nuvio-legsync-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
# R4: uma legenda que nao fecha com nenhuma referencia (tempos irregulares).
python3 - "$FX/ext_ruim.srt" <<'PY'
import sys, random
r = random.Random(7); t = 5.0; o = []
def f(x): return "%02d:%02d:%02d,%03d" % (x // 3600, x // 60 % 60, x % 60, int((x - int(x)) * 1000))
for i in range(1, 161):
    t += 0.4 + r.random() * 5; d = 0.3 + r.random() * 3
    o.append("%d\n%s --> %s\nTexto qualquer %d\n" % (i, f(t), f(t + d), i))
open(sys.argv[1], "w").write("\n".join(o) + "\n")
PY
P=$((20000 + RANDOM % 20000))
D=$(mktemp -d)
python3 tests/legref_rangesrv.py "$FX" $P & S=$!
trap 'kill $S 2>/dev/null || true; rm -rf "$D"' EXIT
for i in $(seq 50); do nc -z 127.0.0.1 $P 2>/dev/null && break; sleep 0.1; done
mkdir -p "$(dirname "${1:-${TMPDIR:-/tmp}/nv-legsync}")"
NV_LEGSYNC_BASE="http://127.0.0.1:$P/f" NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" ${TMPDIR:-/tmp}/nuvio-legsync-shot "$@"
