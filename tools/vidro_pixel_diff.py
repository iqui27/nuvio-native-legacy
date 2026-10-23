#!/usr/bin/env python3
"""Compare 1080p C9 frost shots against the old capture path (ImageMagick)."""
import pathlib
import re
import subprocess
import sys
import tempfile

before, after = map(pathlib.Path, sys.argv[1:3])
failures = 0
shots = sorted(before.glob("*/*-1920.png"))
assert len(shots) == 12, f"Expected 12 C9 1080p shots, found {len(shots)}"
with tempfile.TemporaryDirectory() as work:
    for old in shots:
        relative = old.relative_to(before)
        new = after / relative
        masked = [str(pathlib.Path(work) / f"{i}.png") for i in range(2)]
        # Only wall-clock digits vary independently of the rendered scene.
        for source, target in zip((old, new), masked):
            subprocess.run(["magick", str(source), "-fill", "black", "-draw",
                            "rectangle 1735,25 1840,100", target], check=True)
        result = subprocess.run(["magick", "compare", "-metric", "PAE",
                                 *masked, "null:"], capture_output=True, text=True)
        assert result.returncode in (0, 1), result.stderr
        match = re.search(r"\(([0-9.eE+-]+)\)", result.stderr)
        assert match, result.stderr
        maximum = round(float(match[1]) * 255)
        ok = maximum <= 5
        failures += not ok
        print(f"{'PASS' if ok else 'FAIL'}: {relative} max={maximum} limit=5")
sys.exit(bool(failures))
