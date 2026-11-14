# global-android-env — integration post-merge (2026-11-13)

- Branch: `integracao/2.1`, merge HEAD `cc7cd8ce41bdb32861589bd8c595b91ac403586d` ("Merge agente/21-fv-global-android-env").
- Parents: BASE `e20cf1473e9d8ad430d096e4de10d54ce7200587`; candidate `71a8014c90319431e1f48002b70958f44aa00979` (ancestor of HEAD, verified with `git merge-base --is-ancestor`).
- Diff `git diff e20cf147 HEAD` sha256 `9c67089984fbaebfe6b746b21f6a42bb35523e0822dd9d3f8e18f33158945275` (matches canonical packet).
- Merge evidence: verification digest `8dff3375680af94d5a7a2c4f858fc32a3712e48fa6ed7a235855def3b51194e9`; merge log `global-android-env-merge.log` (merge_rc 0, status passed, postcheck i18n rc 0).
- Review: `fv-android-env-static1` (static, OK, P3 notes only). Runtime verifier: `fv-android-env-runtime1` (verify1 replica, rc 0 for afr_android, android_live_ts, streamfit_passiva_android, conexlim, build-local, i18n).
- Canonical: `global-android-env-canonical2.log` gate passed, 6/6 declared/executed/passed, 0 errors. Original `global-android-env-canonical.log` rc 2 (schema-only failure) retained unchanged; P3 notes F1-F4 kept verbatim in `global-android-env-canonical-packet.log` `lower_priority_notes`.
- Post-merge checks (this job), run from the worktree root under the guard: `env -i` with private postmerge HOME/XDG/data/TMPDIR under `jobtmp/fv-global-android-env/postmerge1`, `NUVIO_PROPERTIES=/dev/null`, `sandbox-exec -f global-gate-local-only.sb` (sha256 `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`), `timeout 900`. Read-only GRADLE_RO_DEP_CACHE (rocache2) and NUVIO_ANDROID_CACHE (acache) reused. Logs in `global-android-env-postmerge-logs/`.
  - `bash tests/afr_android.sh` rc 0 (sha256 `076ae55483d4b86aa44b3be04cd45f402e6a259538393eaa1ed9feadbda89b33`, same as verify run)
  - `bash tests/android_live_ts.sh` rc 0 (sha256 `788d37b7e3803bff86fb200bc788c99102cee02cfc18da8bb6c8bebedf3292bf`; verify run had a different log hash, rc identical)
  - `python3 tests/streamfit_passiva_android.py` rc 0 (sha256 `29f4df71cd7cbb30d197b0d77e36f3167d1861ad3d496e504a279f2fc185cd2b`)
  - `bash tests/conexlim.sh` rc 0 (sha256 `3183e20f5f2bd82555a69a026d0314e5ac83f82469f3efe632946fffc4dcaf4e`)
  - `python3 tools/varredura-i18n.py` rc 0 (sha256 `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1`)
  - Build not repeated (no new source); build-local rc 0 from the verify run.
- Scope: test-only and docs-only. Zero `src/` changes in this merge. Fixture evidence only: NOT a TV fix claim, no device acceptance.
- Still open: full AV/BIB global gate DID RUN rc 1 incomplete (ENOSPC). It remains REQUIRED and unwaived. Optional 55e intermediate Kotlin repro not run. `tests/cacheboost_android.py` stale-deps gap out of this cut. Central `central.py verifica` completed before merge: canonical2 passed 6/6, errors [].
- P3 notes (kept, no change): F1 SDL-missing branch in `tests/streamfit_passiva_android.py:68`; F2 docstring names 4 files at `:3`; F3 partial RO cache StopIteration in `tests/afr_android.py:66` (opt-in); F4 NUVIO_TEST_JAVA wording at `tests/afr_android.py:72`.
