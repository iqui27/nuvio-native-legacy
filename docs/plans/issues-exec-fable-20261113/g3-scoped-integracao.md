# G3 scoped — integration record (2026-11-14)

Test-only infrastructure cut. No `src/` change. Not an issue closure, not a TV proof, not an all-G3-fixed claim.

## Merge
- Merged into `integracao/2.1` at `a7128d7882970ea475eeac629e2085355b2fab07`.
- Candidate `79e4cfddf86bf9db8a156e414d382914e6bc90e5` (branch `agente/21-fv-g3-scoped`, fix commit "Fix G3 test link drift for four guide and DTS targets").
- BASE `46430046d0566edb106a169831f0b08004869070`. diffSHA256 `f2003f32dac18eadce9fb2ad7771b5c9e295be465155841fd659fae85f88865d` (7 test files, +60/-8).
- Verification digest `bc29423cece09d8df09212db3b1e8bde74b365a6f63c319e5efaca47dd3a6833`.
- Merge log: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/g3-scoped-merge.log` (status passed, postcheck passed).

## Review identities
- Static reviewer: `fv-g3-scoped-static1` (nv-revisao, medium). Findings P0-P2: none. Report: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/g3-scoped-code-review.md`.
- Distinct runtime verifier: `fv-g3-scoped-runtime1` (nv-revisao, medium). Findings P0-P2: none. Report: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/g3-scoped-verification.md`.
- Served model for both: UNKNOWN.
- Central canonical gate (`central.py verifica`): 6/6 declared and executed, 0 failed, 0 skipped, 0 timeouts, 0 divergences, `errors: []`. Log: `g3-scoped-canonical.log`.

## Post-merge checks (2026-11-14, HEAD `a7128d78` clean before and after each run)
Run sequentially, foreground, under the job's own wrapper (env -i, private HOME/XDG/TMPDIR/NUVIO_DADOS, `timeout 900`, `sandbox-exec` with pinned guard `global-cw-rewatch-verify1-guard.sb` sha `942c598d…`, original `global-gate-local-only.sb` sha `223ba6e2…` prefix-checked before each run). No build repeated.

| command | rc |
|---|---|
| `bash tests/guiabusca_incremental.sh` | 0 |
| `bash tests/guia_catalogo_tv.sh` | 0 |
| `bash tests/epg_accounts.sh` | 0 |
| `bash tests/dts_edid_pending_lg.sh` | 0 |
| `python3 tools/varredura-i18n.py` | 0 |

Raw logs (out, err, rc, before/after identity, sha256 per run): `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/g3-scoped-postmerge-logs/`. Report: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/g3-scoped-postmerge-report.md`.

## BASE state
- Four targets (`guiabusca_incremental`, `guia_catalogo_tv`, `epg_accounts`, `dts_edid_pending_lg`) were BASE link reds (`ld: symbol(s) not found`), build errors, not behavioral reds. Repaired at `79e4cfdd` by linking the real sources.
- Six other targets were already green at BASE. They are not new fixes.

## Preserved P3 notes (non-blocking, unfixed)
- `tests/dts_edid_pending_lg.c:138` uses `!ajustes_dts_ac3()` (TPK semantics); webOS uses `valor==3`. A constant 0 would be clearer.
- `tests/epg_accounts.sh:19` `-Wno-error` flags apply to every TU; real fix belongs in `src/`.
- `tests/guia_catalogo_tv.c:77` alias goes stale after realloc; harmless now, double free if teardown is added.
- DTS route stub does not record `dts_off`; coverage gap, pre-existing.

## Still open
- `guia_grande` contract failure (cap removed by `a5f0675e5`): UNRESOLVED. Probe rc 134 at `guia_grande.c:36`, reverted.
- `amigos_polish` GUI: UNRESOLVED, not ready.
- Full global AV15fb: rc 1, ENOSPC, incomplete, REQUIRED and unwaived. Not run here.
- Integration full suite: NOT passed.
- CAT700 unmerged, waiting for global.
- No TV or device proof. No issue closed. No merge approval implied.

## Provenance
- Author archive 310 MiB exceeded the 200 MiB budget while it existed. Incident retained. The archive is removed. Distinct archive of BASE sources (177 MiB) was used only by the runtime verifier. Source identity repairs are provenance, not retroactive.

## Validations record
- One record appended to `validacoes` in `docs/issues/mapa.json` (237 → 238). `itens` stays 642. All other JSON values unchanged (canonical JSON hash of everything except `validacoes` identical before and after).
