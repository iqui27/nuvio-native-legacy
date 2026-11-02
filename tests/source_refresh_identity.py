#!/usr/bin/env python3
"""Exercise the real source watcher and addon request policy without SDL/HTTP."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(source, start):
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


app = (ROOT / "src/app.c").read_text()
addons = (ROOT / "src/addons.c").read_text()
with tempfile.TemporaryDirectory(prefix="nuvio-source-refresh-") as directory:
    temp = Path(directory)
    (temp / "source_refresh_target.inc").write_text(
        block(app, app.index("static void idDoAlvo(")))
    (temp / "source_refresh_addons.inc").write_text(
        block(addons, addons.index("static void buscarPedido(")))
    watcher = block(app, app.index('{ static char ultimoAlvo[64] = "";'))
    (temp / "source_refresh_watch.inc").write_text(
        "static void watch(void) {\n" + watcher + "\n}\n")
    flags = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(temp / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-I" + directory,
                    str(ROOT / "tests/source_refresh_identity.c"), "-o", binary], check=True)
    for mode in range(6):
        subprocess.run([binary, str(mode)], check=True)
