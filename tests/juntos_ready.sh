#!/bin/bash
# Watch Together readiness (owner 26/10, cinema mockup): seat states with icon
# and green when ready, a named readiness line, and each TV's state inside the
# shared Sources sheet. Captures side by side with the mockup.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/juntosready
mock=/Volumes/ExternalSSD/tmp/rev/juntosmagia-png/cinema-v2/after
python3 - <<'PY'
from pathlib import Path
ui = Path('src/juntosui.c').read_text()
seat = ui[ui.index('static void desenharLugar('):ui.index('static int todosProntos(const JsMotor *m) {')]
assert 'estadoLinha(p, cx' in seat, 'seat state must be icon + word (green when ready)'
assert 'prontoPulso' not in ui, 'ready glow is still in the mockup, no pulse'
assert '- (1 - chega) * 20' in seat, 'seat arrival comes down 20 px like the mockup'
assert 'anim_rampa(lugarAnim[i], presente, dt, LUGAR_MS)' in ui and '#define LUGAR_MS 860.0f' in ui, 'seat runs on its own timed clock (reduced motion snaps)'
assert 'chega = anim_saida(ms / 600)' in seat and 'luz = anim_suave((ms - 160) / 700)' in seat, '600 ms arrival, then 700 ms light 160 ms in'
assert 'fraseEspera(&m->sala.m[falta])' in ui, 'readiness line names who is still getting ready'
assert 'stream_folha_sala_h = salaFolhaH' in ui, 'Sources sheet must show the room statuses'
st = Path('src/streams.c').read_text()
assert 'folhaDica + folhaSala)' in st and 'stream_folha_sala_desenhar(' in st, 'sheet reserves and draws the room strip'
PY
mkdir -p "$out"
NUVIO_JUNTOS_ART=/Volumes/ExternalSSD/tmp/rev/juntoscinema-nativo-png/v3/assets bash tests/juntos_shot.sh "$out/" --lobby21
rm -f "$out"/*-m*.png
for pair in "02-arrival:100-glass-2-arriving:lobby-not-ready" "03-lobby:100-glass-3-ready:lobby-ready" "20-sources:100-glass-6-sources:sources-statuses"; do
  IFS=: read -r m a name <<<"$pair"
  magick "$mock/$m.png" "$out/$a.png" -resize 1280x720 +append "$out/side-$name.png"
done
echo "juntos_ready: ok ($out)"
