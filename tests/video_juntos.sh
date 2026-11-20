#!/bin/bash
# Focused Watch Together contract plus existing protocol, engine and source checks.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-video-juntos.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-deprecated-declarations -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections src/video.c src/video_modo.c tests/video_juntos.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/test"
"$work/test"
cc -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_cliente.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/client"
"$work/client"
python3 - <<'PY'
from pathlib import Path

app = Path('src/app.c').read_text()
for request in ('episodios_escolheu(&t,&e)', 'player_pediu_proximo(&t,&e)',
                'posplay_pediu_episodio(&t, &e)'):
    block = app[app.index(request):]
    assert block.index('episodioNaSala(titulo, t, e)') < block.index('player_encerrar()'), request
helper = app[app.index('static int episodioNaSala('):app.index('static void alvoPlayer(')]
assert 'if (!juntos_ativo()) return 0;' in helper
assert 'return juntos_ao_proximo(alvo);' in helper
print('episode navigation: picker, next and post-play use room authority before reopening')
PY
bash tests/juntossync.sh
bash tests/juntos_sim.sh
bash tests/juntosfonte.sh
bash tests/video_android_cacheboost.sh
bash tests/tpk-escolha.sh
NV_JUNTOS_TEST=1 bash tests/video_pausa_lg.sh
node tests/tizen-avplay-contract.cjs
bash tests/juntos_e2e.sh
