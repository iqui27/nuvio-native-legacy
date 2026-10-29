#!/bin/bash
# Component parity contract plus real-renderer state and motion proof.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
python3 - <<'PY'
from pathlib import Path
s = Path('src/srvsetup.c').read_text()
def body(start, end):
    return s.split(start, 1)[1].split(end, 1)[0]
panel = body('static void panel(', 'static void logo(')
assert 'plrui_material(' in panel and 'plrui_linha_foco(' in panel, 'panel must use shared material and row focus'
assert 'gfx_cor(' not in panel and 'gfx_sombra_sob(' not in panel, 'custom panel path survives'
button = body('static void button(', 'static void input(')
assert 'plrui_botao(' in button and 'gfx_cor(' not in button, 'idle and focused actions must share the pill renderer'
steps = body('const char *steps[]', 'bodyDrawing = screen != ROWS;')
assert 'plrui_seg(' in steps and 'plrui_trilho(' in steps and 'gfx_cor(' not in steps, 'step selection and completion must use shared segments and progress'
assert 'mov_mola_assenta(' in s and 'anim_mola2_reduzida(' in s and 'anim_mola(' in s
assert 'plrui_dicas(' in s and 'plrui_seg(' in s
print('PASS: wizard shared material, rows, pills, hints, segments, progress and springs')
PY
NUVIO_ESSENCE=1 NUVIO_SHOT_QUICK=1 NUVIO_SHOT_BIN=/Volumes/ExternalSSD/tmp/rev/ess-srvsetup/after-shot \
  bash tests/servidores_setup_shot.sh /Volumes/ExternalSSD/tmp/rev/ess-srvsetup/after
