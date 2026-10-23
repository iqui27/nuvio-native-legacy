#!/bin/bash
# Timeline text must clear the artwork at every text scale (#205).
set -eu
cd "$(dirname "$0")/.."
python3 -I - <<'PY'
import re
import subprocess
import tempfile
from pathlib import Path

src = Path('src/vertudo.c').read_text()
helpers = '\n'.join(re.search(r'static GfxRect ' + name + r'\(.*?\n\}', src, re.S).group()
                    for name in ('vtTimelineCard', 'vtTimelineArte'))
text_column = re.search(r'  float tx=.*?\n  float tw=.*?;', src).group()
c = '''#include <math.h>
#include <stdio.h>
typedef struct { float x, y, w, h; } GfxRect;
static float scale;
static int phone, portrait;
static float gfx_escala_txt(void) { return scale; }
static int vtTelefone(void) { return phone; }
static int vtTimelineRetrato(void) { return phone && portrait; }
static float vtInicio(void) { return 200.0f; }
static float vtFim(void) { return (phone ? 1920.0f : 1536.0f) / scale; }
#define VT_CARD_H 236.0f
''' + helpers + '''
int main(void) {
  const float scales[] = {0.85f, 1.0f, 1.15f};
  int failed = 0;
  for (int device = 0; device < 3; ++device) {
    phone = device != 0; portrait = device == 2;
    for (int i = 0; i < 3; ++i) {
      scale = scales[i];
      GfxRect card = vtTimelineCard(332.0f), artRect = vtTimelineArte(card);
''' + text_column + '''
      float gap = tx - artRect.x - artRect.w;
      int ok = gap >= 24.0f - 0.001f && tw > 0 &&
               tx + tw <= card.x + card.w &&
               fabsf(card.h - 236.0f) < 0.001f &&
               fabsf(artRect.h / artRect.w - 212.0f / 376.0f) < 0.001f;
      printf("%s device=%d scale=%.2f gap=%.2f width=%.2f\\n",
             ok ? "PASS" : "FAIL", device, scale, gap, tw);
      failed |= !ok;
    }
  }
  return failed;
}
'''
with tempfile.TemporaryDirectory(prefix='timeline-text-') as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(c)
    subprocess.run(['cc', '-Wall', '-Wextra', str(source), '-o', str(binary), '-lm'], check=True)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
PY
