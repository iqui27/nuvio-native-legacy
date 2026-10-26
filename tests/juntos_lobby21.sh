#!/bin/bash
# Full-bleed Watch Together lobby (owner round 26/10): states at 100% glass and
# 85% solid, invite panel, shared Sources sheet, D-pad, plus a motion strip.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/juntoslobby
export NUVIO_JUNTOS_ART=/Volumes/ExternalSSD/tmp/rev/juntoscinema-nativo-png/v3/assets
mkdir -p "$out"
# Sources: the lobby stays under the shared sheet while it opens and closes.
python3 - <<'PY'
from pathlib import Path
ui = Path('src/juntosui.c').read_text()
draw = ui[ui.index('void juntosui_desenhar(Uint32 agora) {'):][:200]
assert 'stream_folha_aberta()' not in draw, 'lobby must not vanish while the Sources sheet is up'
app = Path('src/app.c').read_text()
under = app.index('stream_folha_anim() > .005f) {')
assert under < app.index('juntosui_desenhar(agora);') < app.index('stream_folha_desenhar(agora);'), 'lobby must draw before (under) the Sources sheet'
assert 'stream_folha_anim() <= .005f) juntosui_desenhar(agora);' in app, 'lobby must not redraw over a closing sheet'
PY
bash tests/juntos_shot.sh "$out/" --lobby21
for tag in 100-glass 85-solid; do
  magick "$out/$tag"-m*.png -resize 640x360 +append "$out/$tag-strip-motion.png"
  rm "$out/$tag"-m*.png
done
for image in "$out"/*.png; do
  magick "$image" -format '%[fx:standard_deviation>0.05]' info: | grep -qx 1
done
echo "juntos_lobby21: ok ($out)"
