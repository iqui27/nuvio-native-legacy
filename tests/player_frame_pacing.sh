#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/check.c" <<'C'
#include "player_pacing.h"
#include <assert.h>
int main(void) {
  assert(!player_ui_frame(0, 30, 10));
  assert(!player_ui_frame(1, 1, 0));
  assert(!player_ui_frame(1, 1, 1)); /* hole plus its scissored clear */
  assert(player_ui_frame(1, 2, 0)); /* controls, clock, toast or subtitle */
  assert(player_ui_frame(1, 1, 2)); /* non-rectangular UI beyond hole clear */
  assert(player_ui_frame(1, 0, 1));
  assert(player_frame_wait(0, 0) == 0);
  assert(player_frame_wait(1, 0) == 16);
  assert(player_frame_wait(1, 10) == 6);
  assert(player_frame_wait(1, 16) == 0);
  assert(player_frame_wait(1, 200) == 0);
  return 0;
}
C
for platform in NV_WEBOS NV_TPK NV_ANDROID __EMSCRIPTEN__; do
  cc -std=c99 -Wall -Wextra -Werror -D"$platform" -Isrc "$tmp/check.c" -o "$tmp/check"
  "$tmp/check"
done
python3 - <<'PY'
from pathlib import Path
s = Path('src/main.c').read_text()
assert 'player_ui_frame(player_aberto(), gfx_n_rect, gfx_n_outros)' in s
assert 'nv_ceder_quadro(playerUi)' in s
assert 'SDL_GL_SetSwapInterval(playerUi ? 0 : 1)' in s
assert 'player_frame_wait(playerUi,' in s
assert 'if (quadros > 20 || dtms > 1000.0)' not in s
h = Path('src/tpk.h').read_text()
assert '#define SDL_GL_SetSwapInterval(i)          tpk_gl_intervalo(i)' in h
c = Path('src/tpk.c').read_text()
assert 'swapZero ? 0 : intervalo' in c
print('PASS: shared player pacing on webOS, TPK, Android and WGT; slow samples retained')
PY
