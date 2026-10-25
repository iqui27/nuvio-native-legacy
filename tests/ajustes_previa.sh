#!/bin/bash
# Preview illustrations must never duplicate option-row controls.
# Optional revision verifies the same contract against an earlier tree.
set -eu
cd "$(dirname "$0")/.."
python3 - "$@" <<'PY'
from pathlib import Path
import re
import subprocess
import sys

paths = [Path("src/ajcena.inc"), *sorted(Path("src").glob("ajustes_ux_cenas*.inc"))]
forbidden = re.compile(r'\b(?:ajc_chave|ajcChaveCanto)\s*\(')
failures = []
for path in paths:
    source = (subprocess.check_output(["git", "show", f"{sys.argv[1]}:{path}"], text=True)
              if len(sys.argv) > 1 else path.read_text())
    failures.extend(f"{path}:{line}: {text.strip()}"
                    for line, text in enumerate(source.splitlines(), 1) if forbidden.search(text))
    if path.name == "ajustes_ux_cenas_reproducao.inc":
        priority = source.split("static void rPrioridade(void)", 1)[1].split("static void rPrazo(void)", 1)[0]
        if '"aj_sliders-horizontal"' in priority:
            failures.append(f"{path}: source priority preview contains a settings button")
if failures:
    print("FAIL: controls inside preview illustrations\n" + "\n".join(failures))
    sys.exit(1)
print(f"PASS: {len(paths)} preview sources contain no switches or source-selection button.")
PY
