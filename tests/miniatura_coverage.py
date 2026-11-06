"""Compile the pure production coverage helper with address/UB sanitizers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    executable = Path(directory) / "coverage"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(root / "tests/miniatura_coverage.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
