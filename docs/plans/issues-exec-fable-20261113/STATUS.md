# Campaign status — issues-exec-fable-20261113

Scope: TV only. Original 118 planned IDs; 114 in TV scope, 4 outside (inactive).
Authorized by owner 2026-11-13. Base: `9115d8bbadd09f98fef5b939732d5334d7b726c7`.

## Merged
- #773 — first code cut merged into `integracao/2.1` at `261ab41c90838b86c0cdf40811a5ffc2ece08fe1` (source `1daef874bf1dce36dc74dc2dc2200f1ac03c5dfd`). Gate digest `a1202ed80b0b73cbe93481e5ffea52fbf0ee6607d9cc1a0c2dbc3da8e7a34ceb`, 2/2 passed. Issue OPEN. Details: `leg773-integracao.md`.
- No other campaign candidate merged. Central holds further merges until post-merge docs are done.

## Evidence for #773
- Red on base: rc 134, assertion at `tests/video_acb_lg.c:196`. Green on source head: rc 0; build rc 0.
- Post-merge `tests/video_acb_lg.sh` on `261ab41`: rc 0, log SHA-256 `68be098982c7a41263a09e44a1555a135404556fbdf003905df24527d9db6a55`.
- i18n post-check rc 0.
- Reviewer: `j-rev-fv-leg773-code`, model UNKNOWN. Verifier: `j-verify-fv-leg773`, model UNKNOWN.

## Open / unproven
- #773: B8 (webOS 4.4.3) device proof and on-device selection still missing. Not claimed fixed on device.
- P2 notes F1–F5 (see `leg773-integracao.md`): kept, not blockers.
- All other issues: NOT integrated, pending candidate flow.
