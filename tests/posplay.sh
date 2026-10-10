#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
flags=(-DNV_SHOT_HOOKS)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Liga o app inteiro menos o main, como tests/player.sh: posplay.c fala com
# catalogo, extras e video. Nenhuma janela e aberta — posplay_atualizar nao
# desenha, e e so ele e posplay_evento que este teste exercita.
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/posplay.c -Isrc -o /tmp/nuvio-posplay-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-posplay-tests "$@"

# #177: o cartao do proximo episodio desfoca o still pela mesma regra do detalhe.
# O desenho nao roda sem janela; aqui a guarda e estatica, para ninguem tirar.
grep -q 'ajustes_desfocar_nao_assistidos()' src/posplay.c
grep -q 'gfx_desfocado(t, arte)' src/posplay.c
echo "posplay: desfoque do proximo episodio presente (#177)"

# #232: do not feed an already blurred texture back into the blur cache.
python3 - <<'PYTEST'
from pathlib import Path
s = Path('src/posplay.c').read_text()
draw = s.split('void posplay_desenhar(', 1)[1]
assert draw.count('gfx_desfocado(t, arte)') == 1
assert 'if (t && posplay_desfocar_thumb(idx, proxT, proxE))' in draw
print('posplay: next episode thumbnail uses one guarded blur pass (#232)')
PYTEST

# 2.0.3: a legenda principal sobe pelo CARTAO na tela, nunca pela janela.
python3 - <<'PYTEST'
from pathlib import Path
s = Path('src/player.c').read_text()
corpo = s.split('static float baseLegendaPrincipal(void) {', 1)[1].split('\n}', 1)[0]
assert 'ofertaProximo' not in corpo, 'a legenda voltou a subir pela janela do cartao'
assert 'posplay_sobre_video()' in corpo
PYTEST
echo "posplay: legenda segue o cartao, nao a janela (2.0.3)"
