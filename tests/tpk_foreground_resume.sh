#!/bin/bash
# #799: TPK precisa avisar o main() ao voltar do segundo plano (SDL_APP_DIDENTERFOREGROUND).
set -euo pipefail
cd "$(dirname "$0")/.."

grep -q 'nv_tpk_retomou' src/tpk.c
grep -q 'SDL_APP_DIDENTERFOREGROUND' src/tpk.c
grep -q 'SDL_PushEvent' src/tpk.c
grep -q 'nv_tpk_retomou' tizen-tpk/Program.cs

python3 - <<'PY'
from pathlib import Path
src = Path("tizen-tpk/Program.cs").read_text()
resume = src.split("protected override void OnResume()", 1)[1].split("protected override", 1)[0]
assert "nv_tpk_retomou" in resume
assert resume.index("RetomarDoSistema") < resume.index("nv_tpk_retomou")
print("PASS: OnResume avisa o C com DIDENTERFOREGROUND (#799)")
PY
