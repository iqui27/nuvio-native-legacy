#!/usr/bin/env python3
"""Check Standard footer geometry using the production draw expressions."""
import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
detail = (root / 'src/detail.c').read_text()
header = (root / 'src/detail.h').read_text()
meta = re.search(r'float x = tx, y = r.y \+ \(([^;]+)\);', detail)[1]
bar = re.search(r'GfxRect tr = \{ tx, r.y \+ \(([^;]+)\), epTextoW', detail)[1]
defines = '\n'.join(line for line in header.splitlines()
                    if re.match(r'#define NV_DETP_(EP_(THUMB_H|META_Y|BARRA_Y|ICONE|BARRA_H)|EPM_(META_DY|BARRA_DY))\s', line))
code = '''#include <stdio.h>
#include <math.h>
''' + defines + '''
static float scale;
static float epThumbH(void) { return NV_DETP_EP_THUMB_H / scale; }
int main(void) {
  int moderna = 0, bad = 0;
  float scales[] = {.85f, 1.f, 1.3f};
  for (int i = 0; i < 3; i++) {
    scale = scales[i];
    float meta = (''' + meta + ''');
    float bar = (''' + bar + ''');
    int fail = meta < 0 || meta + NV_DETP_EP_ICONE > bar ||
               bar + NV_DETP_EP_BARRA_H > epThumbH();
    if (scale == 1) fail |= fabsf(meta - NV_DETP_EP_META_Y) > .001f ||
                           fabsf(bar - NV_DETP_EP_BARRA_Y) > .001f;
    printf("%s Standard %.0f%%: metadata=%.1f progress=%.1f artwork=%.1f\\n",
           fail ? "FAIL" : "PASS", scale * 100, meta * scale,
           bar * scale, epThumbH() * scale);
    bad |= fail;
  }
  return bad;
}
'''
with tempfile.TemporaryDirectory(prefix='nv-rev6-') as tmp:
    path = pathlib.Path(tmp)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    raise SystemExit(subprocess.run([str(path / 'test')]).returncode)
