#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# O envio de registro usa este helper tanto no nativo quanto no WGT.
python3 - <<'PY'
import re
import subprocess

for target in ([], ["-D__EMSCRIPTEN__", "-Itests/wgt_stub"]):
    source = subprocess.run(
        ["cc", "-E", "-P", *target, "-Isrc", "src/rede.c"],
        capture_output=True, text=True, check=True,
    ).stdout
    definitions = re.findall(
        r"char\s*\*\s*rede_postar_repetindo\s*\([^;{}]*\)\s*\{", source
    )
    name = "WASM" if target else "native"
    assert len(definitions) == 1, f"{name}: expected one retry helper, got {len(definitions)}"
    print(f"PASS: {name} defines rede_postar_repetindo")
PY
