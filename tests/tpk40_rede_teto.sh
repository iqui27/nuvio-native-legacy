#!/bin/bash
# Offline regression proof for the Tizen 4/5 object selection and link guards.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
import subprocess

script = Path("tools/tpk.sh").read_text()
selection = next(line.strip() for line in script.splitlines()
                 if line.strip().startswith("SOURCES40="))
selected = set(subprocess.check_output(
    ["bash", "-eu", "-c", selection + '\nprintf "%s\\n" "$SOURCES40"'],
    text=True).splitlines())
sources = list(Path("src").glob("*.c")) + list(Path("src/dts").glob("*.c"))
users = {str(path) for path in sources
         if re.search(r"\brede_teto\b", path.read_text())}
assert "src/tex_cache.c" in users, "Missing regression trigger"
assert users <= selected, f"TLS users reused in tpk40: {sorted(users - selected)}"
assert "src/video_tizen.c" not in selected, "Wrong video backend selected"

# Recompiled users must receive the macro, and unresolved symbols must fail link.
block = script[script.index("  SOURCES40="):script.index('\n  echo "  $(ls',
                                                         script.index("  SOURCES40="))]
assert 'NATIVE_FLAGS="$FLAGS -DNV_TPK40 ' in block
assert re.search(r"gcc -shared -o /work/build/tpk/libnuvio-tpk40\.so .*--no-undefined", block)
# Existing artifact guards reject compiler TLS even when its symbols resolve.
assert 'readelf -lW "$L40"' in script and 'TLS\\s' in script
assert 'readelf -rW "$L40"' in script and 'R_ARM_TLS_' in script
assert 'readelf -sW --dyn-syms "$L40" | grep -q "__tls_get_addr"' in script
print("PASS: rede_teto users recompiled with NV_TPK40; undefined-symbol and TLS guards retained")
print("Offline script proof only; ARM artifact nm check requires a Tizen build")
PY
