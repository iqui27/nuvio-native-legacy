#!/usr/bin/env python3
"""Check synchronized keys and require all full-validator errors to predate this change."""
import subprocess
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import idiomas

master = idiomas.ler_mestra()
keys = [idiomas.decodificar(pt) for _, pt, _ in master]
assert keys == sorted(set(keys))
for language in idiomas.IDIOMAS:
    assert [pt for _, pt, _ in idiomas.ler_irma(language)] == [pt for _, pt, _ in master]

# The repository's existing Serbian table includes Cyrillic translations.
# Prove those rows are unchanged instead of accepting new validator failures.
_, errors = idiomas.conferir()
old = subprocess.check_output(["git", "show", "HEAD:src/idioma_sr.h"], text=True).splitlines()
current = (idiomas.SRC / "idioma_sr.h").read_text().splitlines()
for error in errors:
    assert error.startswith("idioma_sr.h:") and "cirilico" in error, error
    line = int(error.split(":")[1])
    assert current[line - 1] in old, error
print(f"PASS: sorted keys, all language tables synchronized; {len(errors)} unchanged Serbian validation issues")
