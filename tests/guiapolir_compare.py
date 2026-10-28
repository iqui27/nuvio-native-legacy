#!/usr/bin/env python3
"""Place baseline and current native captures side by side using the existing image tool."""
import subprocess
import sys
from pathlib import Path

root = Path(sys.argv[1])
for path in (root / "after").glob("*-app.png"):
    info = subprocess.check_output(["magick", str(path), "-format", "%w %h %[fx:standard_deviation]", "info:"], text=True).split()
    assert info[:2] == ["1920", "1080"] and float(info[2]) > .05, path
for name in ("options-first", "home-multiview", "multiview-slot", "preview-card", "preview-large"):
    before = root / "before" / f"{name}-app.png"
    after = root / "after" / f"{name}-app.png"
    if before.exists():
        subprocess.run(["magick", str(before), str(after), "+append", str(root / f"{name}-compare.png")], check=True)
print("PASS: nonblank native captures and before | after comparisons")
