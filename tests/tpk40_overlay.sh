#!/bin/bash
# Tizen 4.0: "chuvisco" sobre o video = overlay GL nao cobre a superficie inteira
# ou arranque com clear opaco/lixo. Contrato em src/tpk.c (sem TV).
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re, sys
from pathlib import Path
tpk = Path("src/tpk.c").read_text()
hdr = Path("src/tpk_egl.h").read_text()
fail = []
if "QuerySurface" not in hdr:
    fail.append("tpk_egl.h: falta eglQuerySurface")
if 'PEGA(QuerySurface, "eglQuerySurface")' not in tpk:
    fail.append("tpk.c: nao carrega eglQuerySurface")
if "tela adotada da superficie" not in tpk:
    fail.append("tpk.c: nao adota EGL_WIDTH/HEIGHT da superficie")
if "[tpk] egl config id=" not in tpk:
    fail.append("tpk.c: falta log da config EGL (alfa)")
# Bloco NV_TPK40 no arranque (esperaMs < 0) tem de limpar com alfa 0.
m = re.search(
    r"#ifdef NV_TPK40\s*\n(.*?)#else",
    tpk,
    re.S,
)
if not m or "glClearColor(0, 0, 0, 0)" not in m.group(1):
    fail.append("tpk.c: NV_TPK40 nao faz glClearColor(0,0,0,0) no arranque")
if fail:
    for f in fail:
        print("FALHA:", f)
    sys.exit(1)
print("PASS: overlay Tizen 4 adota superficie, loga EGL alfa e clear transparente")
PY
