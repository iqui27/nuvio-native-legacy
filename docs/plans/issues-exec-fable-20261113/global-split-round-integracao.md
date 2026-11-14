# global-split-round — integration post-merge (2026-11-13)

- Branch: `integracao/2.1`, merge HEAD `69a04b58edcfbe081344d43067116d1e6acff06b` ("Merge agente/21-fv-global-split-round").
- BASE `50cc563b2202aa220872bcab5e96e1191afdd610`; candidate `a7486c40daead0a0eabaa762739f4dd1c3016a27` (ancestor of HEAD; refresh merge; author commit `a09ffb3a3`).
- Diff: `git diff 50cc563b2 HEAD` sha256 `36298c260537b03b94c4617b230ae9bd579914e56dcff0dd1c851b51ba3ff2bc`.
- Files: `tests/divididosplit.sh`, `tests/fileiraredonda.sh`, `tests/fileiraredonda.c`. Zero `src/` changes.
- Review: `fv-global-split-static1` (static); runtime verifier `fv-global-split-runtime1`; canonical 4/4 (per central prompt; see `global-split-round-verification.md`).
- Post-merge checks, run from the worktree root under `sandbox-exec -f global-gate-local-only.sb` (sha256 `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`), env `-i`, `NUVIO_PROPERTIES=/dev/null`, timeout 900 each:
  - `bash tests/divididosplit.sh` rc 0
  - `bash tests/fileiraredonda.sh` rc 0
  - `python3 tools/varredura-i18n.py` rc 0 (all counts 0)
- Logs: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/postmerge2-logs/`
- Not claimed: no issue closed; no physical TV acceptance (UNKNOWN); no campaign completion.
- AV/BIB global gate: NOT RUN in this post-merge. Last global run was rc1 incomplete (ENOSPC). Remains REQUIRED and unwaived.
- P3 limits: fixture-only change; coverage limited to the two test files.
