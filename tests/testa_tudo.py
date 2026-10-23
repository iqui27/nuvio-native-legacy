#!/usr/bin/env python3
"""Regression: the suite and sanitizer gate work from outside the repo."""
from pathlib import Path
import shutil
import subprocess
import tempfile

runner = Path(__file__).resolve().parents[1] / "tools/testa-tudo.sh"
with tempfile.TemporaryDirectory(prefix="nuvio-testa-tudo-") as tmp:
    root = Path(tmp) / "repo"
    for directory in (root / "tools", root / "tests", Path(tmp) / "outside"):
        directory.mkdir(parents=True)
    shutil.copyfile(runner, root / "tools/testa-tudo.sh")
    (root / "tests/probe.sh").write_text('test -f tools/testa-tudo.sh && echo suite-probe\n')
    gate = root / "tools/sanitiza.sh"
    gate.write_text('test -f tests/probe.sh && echo sanitizer-probe\n')
    for args, expected_rc, expected_gate in (([], 0, False), (["--sanitizers"], 0, True), (["--sanitizers"], 1, True)):
        if expected_rc:
            gate.write_text('echo sanitizer-probe; exit 1\n')
        result = subprocess.run(["bash", str(root / "tools/testa-tudo.sh"), *args],
                                cwd=Path(tmp) / "outside", capture_output=True, text=True)
        assert result.returncode == expected_rc, result.stdout + result.stderr
        assert "1 passaram" in result.stdout, result.stdout
        assert ("sanitizer-probe" in result.stdout) == expected_gate, result.stdout
        logs = Path(result.stdout.split("logs: ")[-1].strip())
        shutil.rmtree(logs)
print("ok: outside-root suite, sanitizer dispatch and failure propagation")
