# Offline-fixtures — post-merge into integracao/2.1

Scope: test fixtures only (tests/ 11 files in BASE..HEAD). No production fix. Issues #639 and #789 get fixture-infrastructure evidence only, not production closure. BIB639 / PAG789 PRODUCTION candidate is still NOT integrated; full global gate is mandatory and NOT RUN; physical acceptance UNKNOWN. No issue is marked fixed or closed.

## Refs
- BASE: `82af242653ec035a71857b442bea9be7abb473fc` (integracao/2.1 before merge)
- CANDIDATE: `3e509a3bc09ddf8c77d627542f42b286fb918a37` (agente/21-fv-offline-fixtures, 1 commit after fix3)
- MERGE HEAD: `b9e9dfa7a1922ae15990c56e5c618ebae5ce46bd`, parents exactly BASE and CANDIDATE (asserted in post-merge; worktree clean at HEAD before tests)
- `git diff BASE HEAD | shasum -a 256` = `4aa1dc7362f67d8009c9f790c9027d3d92817d1a8c2a345b1870d0c0e2602330` (matches the full diff recorded at review and merge)
- Canonical replay: 5/5 checks passed, 0 skipped / failures / timeouts / divergences / errors; verification digest `3d2c951660b1d478fd82eafc0e35330ef75372f0d9ec1e00499a177ce8e04696`.
- Integrator: merge rc 0, status passed. Post-merge i18n stdout hash `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1`.
- Reviewer (static, source/diff only): `j-rev-fv-offline-fixtures2-static` (nv-revisao medium). Served model UNKNOWN.
- Verifier (runtime, distinct from author and reviewer): `fv-offline-fixtures-runtime-verifier` (job `j-verify-fv-offline-fixtures`). Served model UNKNOWN.
- Author (fix3): `j-fix-fv-offline-fixtures-fix3`.

## Runtime evidence (distinct runtime rc 0)
- Canonical replay under env -i + `sandbox-exec` with `global-gate-local-only.sb` (sha256 `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`), synthetic HOME/XDG, `NUVIO_PROPERTIES=/dev/null`:
  - `bash tests/conta.sh` rc 0
  - `bash tests/trailerapple.sh` rc 0
  - `bash tests/trailerimdb.sh` rc 0
  - `bash tools/build-local.sh` rc 0
  - `python3 tools/varredura-i18n.py` rc 0
- Post-merge re-run by integrator (same guard, separate scratch under `/Volumes/ExternalSSD/tmp/jobtmp/fv-offline-fixtures-postmerge`): `bash tests/conta.sh` rc 0, `bash tests/trailerapple.sh` rc 0.
- OS guard vs egress: the sandbox profile denies network except loopback, so the fixture-only chain could not reach the internet. This proves local containment of the run, not zero egress from the test code itself. The fixture log only sees requests that REACH the fixture (known route + loopback Host).
- No JWT in any log.

## What the fixes changed (tests only)
- Temporary account / session / logout isolation: HOME and XDG under a per-run temp dir; proxy vars unset; `conta_logout.c` fails closed (rc 2) if the data dir is outside the temp root.
- Exact synthetic owner JSON for `get_sync_owner`; owner assert now exact-match against the seeded uuid.
- Hard Apple-encoded query for Tom & Jerry (`&searchTerm=Tom%20%26%20Jerry`); loose matches removed.
- Real Apple / IMDb parsers and selection tests kept; three Android wrappers unchanged; caller `properties` override defaults unchanged.
- Prior false JADE failures: 1 -> 0. All assertions retained.
- Missing `dados_iniciar` in the fixture was the appearance-migration root cause. No production theme defect.

## Known limits (retained, not fixed here)
- F5 P2 (confirmed, non-blocking): fixture JWT `sub` equals USUARIO equals `get_sync_owner` reply. `src/perfis.c:150-154` falls back to token `sub` if the RPC fails, so a regression that never calls get_sync_owner still passes the owner assert. Only a wrong non-empty parse is caught. Source: `tests/nuvem_fixture.py:18,30,73`, `tests/conta_logout.c:83`.
- P3 note (retained): `tests/conta_ajustes.c:64` `rm -rf '%s'` breaks if TMPDIR contains a single quote; own mkdtemp path only.
- F5 / P3 do not block.
- QR check PULADO (no opencv venv): not a complete QR proof.
- Fixture-only cut: not all fixture branches, not global gate, not devices, not production cache.
- The new code does NOT fix all account/sync bugs and does NOT prove TV behavior.
- F5P2 RPC sub-fallback vacuity and P3 quoting remain open.

## Operational support (tooling, not app code)
- `global-gate-support-review.md`: independent review, VEREDITO OK. Scope: `bash tools/testa-tudo.sh` exact command gets acceptance and a 14400-second timeout; all other checks keep 900 seconds.
- `global-gate-support-applied.md`: the reviewed four-hunk patch was applied to the central tool (`central-jobs/scripts/central.py`), candidate sha256 `0b3f2f79ede0e5a6923b7ae5a28e3adbe2e915cb9035cd74d07eb478e6c66a06`.
- This support is NOT an app change and NOT a global pass. The campaign must still retain the actual global summary and limits; rc 0 is not universal coverage. No private patch needed to be copied into the repo.

## STATUS
- Merge `b9e9dfa7a` into integracao/2.1: DONE (post-merge verified).
- Fixture-only infrastructure for #639 / #789: IN integracao/2.1 as test code.
- BIB639 / PAG789 production candidate: NOT integrated.
- Full global gate (BIB/PAG): REQUIRED, NOT RUN.
- Physical acceptance: UNKNOWN.
- Issues #639 / #789: NOT closed, NOT fixed.
- Post-merge stage: `veredito: null`; no new source approval.
