# global-link-cw — integration record (test-only, 2026-11-14)

Scope: test-only merge into `integracao/2.1`. No production fix, no TV proof, no issue closed.

## Merge
- Branch `agente/21-fv-global-link-cw` merged into `integracao/2.1`.
- Merge commit: `d92ce40d092a8c3b6f0c6345f37afcf0645f581d` (BASE `b5e134d3d024d29e9e45aaa223a3c286cc6d1649`, candidate `527569d933390f350e892212de6d4756b1a9505a`).
- Diff `git diff BASE candidate` sha256: `2f7e3071e75c1c8301fb2ac68b76bd35e5f5ab4f14a04bf012399a768722ba5a`.
- Verification digest: `afefb8bc5163694bbe6294e471d3c59e10615f60bdafad644996c12b362f126e`.
- Files: `tests/cw205_diagnostico.sh`, `tests/cwleg.sh`, `tests/trakt_rede_stub.c` and fixtures. Zero production `src/` behavior change claimed by this record.

## Reviewers and identities (kept separate)
- Static reviewer: `fv-link-cw-static1` (`global-link-cw-code-review.md`).
- Distinct runtime verifier: `fv-link-cw-runtime1` (`global-link-cw-verification.md`, fresh wrapper and raw logs under `global-link-cw-verify-logs/`).
- Canonical tool: `central.py verifica` (`global-link-cw-canonical.log`): gate passed, 4 declared / 4 executed / 4 passed, 0 skipped, 0 timeouts, errors `[]`.
- Author guarded execution: UNVERIFIED (not reused as proof).

## Post-merge checks (this record)
- Executor: nv-mec low, served model UNKNOWN. Wrapper `global-link-cw-postmerge-wrap.sh` (sha256 `b8829076bab14d82b3b559d622defe0b0f348747cd2eae3574d7b60aa7fe1731`), pin `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`, env-i contract, `timeout 900 sandbox-exec`, sequential foreground.
- `bash tests/cw205_diagnostico.sh` rc 0 (`PASS: 12 pausados preservados`).
- `bash tests/cwleg.sh` rc 0.
- `python3 tools/varredura-i18n.py` rc 0 (stdout sha256 `e2dc1d85…f126e`, matches verifier).
- Raw logs: `global-link-cw-postmerge-logs/pm{1-cw205,2-cwleg,3-varredura}.{stdout,stderr,meta}`.
- Build not repeated (source unchanged in this record).

## Confirmed P2, retained, unfixed (owner P2 non-blocking policy; not waived)
- F1 `tests/trakt_rede_stub.c:12`: `rede_pedir` returns success on the unused paged `provedorGet` path; fixture body discarded. Latent only; no current false pass.
- F2 `tests/cw205_diagnostico.sh:20`: historical replay `CW205_REF=581e8fb9`. BASE: behavioral FAIL (`trakt_continuar descartou filmes pausados a 62% por sugestoes`). HEAD: compile failure (`src/metaep.c` missing at that ref). Nonzero historical replay is expected and is not a required check.

## Red distinctions
- BASE link-stage red (undefined `metaep_*`, `rede_*`, `fil_*` symbols, ld arm64): compile/link red, not behavioral.
- F2 BASE replay: behavioral red. F2 HEAD replay: compile red. Keep these distinct.

## Limits
- Full global AV/BIB run: last campaign on AV candidate `15fbcbf2dcd5a7a8697a36cd9229b3963d4514ab` ended rc 1 incomplete (ENOSPC). Not passed. Required and unwaived; not run here.
- No TV or device proof. No issue closed. No status reclassification.
