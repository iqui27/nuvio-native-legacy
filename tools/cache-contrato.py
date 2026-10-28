#!/usr/bin/env python3
"""Budget/doc coupling, for working trees and each committed budget revision."""
import hashlib
from pathlib import Path
import re
import subprocess

SOURCE = "src/perfiltv.c"
DOC = "docs/cache-capas.md"
MARKER = r"perfiltv-sha256: ([0-9a-f]{64})"


def git(*args):
    return subprocess.check_output(["git", *args], text=True)


def digest(source):
    return hashlib.sha256(source.encode()).hexdigest()


def check(source, doc, where):
    marker = re.search(MARKER, doc)
    if not marker or marker[1] != digest(source):
        raise SystemExit(f"FAIL: {where}: update {DOC} budget table and perfiltv-sha256")


check(Path(SOURCE).read_text(), Path(DOC).read_text(), "working tree")
# Check every source revision since the standard was introduced, not merely HEAD.
intro = git("log", "--reverse", "--format=%H", "--diff-filter=A", "--", DOC).splitlines()
if intro:
    for rev in git("rev-list", f"{intro[0]}^..HEAD", "--", SOURCE).splitlines():
        doc = git("show", f"{rev}:{DOC}")
        check(git("show", f"{rev}:{SOURCE}"), doc, rev[:9])
        parents = git("rev-list", "--parents", "-n", "1", rev).split()[1:]
        if len(parents) == 1:
            changed = git("diff-tree", "--no-commit-id", "--name-only", "-r", rev).splitlines()
            if DOC not in changed:
                raise SystemExit(f"FAIL: {rev[:9]} changed budgets without {DOC} in same commit")
print("PASS: budget/document SHA256 and same-commit history")
