# Issue #773 — integration record (webOS subtitle trackNum)

Status: code merged into `integracao/2.1`. Issue stays OPEN. Device proof missing.

## Provenance
- Base: `9115d8bbadd09f98fef5b939732d5334d7b726c7`
- Source head (reviewed): `1daef874bf1dce36dc74dc2dc2200f1ac03c5dfd` ("webOS: bound trackNum parse, positional fallback when all missing (#773)")
- Merge into integration: `261ab41c90838b86c0cdf40811a5ffc2ece08fe1` ("Merge agente/21-21-fv-leg-diag"), parents `9115d8bba` and `1daef874b`.
- Diff `9115d8bba..1daef874b`: 2 files, `src/video.c` +20/-1, `tests/video_acb_lg.c` +53. Diff SHA-256 `4d856f16b388b84f26d7d682ea693e1705a75ccd28fef83ba02712d6b2cd4539`.
- Gate digest (evidence): `a1202ed80b0b73cbe93481e5ffea52fbf0ee6607d9cc1a0c2dbc3da8e7a34ceb`.
- Gate: 2/2 passed (independent static review + distinct runtime verifier). Integrator i18n post-check rc 0, status passed.

## Verification (distinct verifier, pre-merge)
- Red on base: `tests/video_acb_lg.sh` rc 134, assertion `nLeg==2 && faixaLeg[0].numero==0 && faixaLeg[1].numero==1` at `video_acb_lg.c:196`.
- Green on source head: rc 0. `tools/build-local.sh` rc 0.
- Findings: F1 P2 nested key borrowed (`src/video.c:1272`), F2 P2 quoted numerics accepted (`src/video.c:1272`), F3 P2 INT_MAX label overflow (`src/video.c:1290`, pre-existing), F4 P2 mixed invalid labels "Legenda 0" (`src/video.c:1290`), F5 P2 missing test cases (`tests/video_acb_lg.c:205`). No P0/P1.

## Post-merge check (integration head `261ab41`)
- `TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp/fv-leg773-postmerge NUVIO_PROPERTIES=/dev/null bash tests/video_acb_lg.sh`: rc 0, log SHA-256 `68be098982c7a41263a09e44a1555a135404556fbdf003905df24527d9db6a55`.
- Head unchanged before and after the run.

## Reviewer / verifier roles
- Static reviewer: job `j-rev-fv-leg773-code` (role `fv-leg773-static-review`). Served model UNKNOWN (no gateway proof).
- Runtime verifier: job `j-verify-fv-leg773` (role `fv-leg773-runtime-verifier`). Served model UNKNOWN (no gateway proof).

## Not proven
- B8 (webOS 4.4.3) device payload shape and on-device track selection: unproven.
- #773 is a code fix for the trackNum parse path. It does NOT mean all subtitles or TV subtitle selection are fixed.
- P2 notes F1–F5 are open notes, not blockers.
