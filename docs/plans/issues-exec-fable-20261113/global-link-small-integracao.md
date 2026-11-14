# global-link-small: integration record (fixture-only)

Date: 2026-11-13. Branch: integracao/2.1. Scope: test fixtures only; no production change; no issue closed or reclassified.

## Identity
- BASE (integration before merge): 104aebf52c9b1d9b1c7d01f69dc10e564f45fcfa
- Candidate: f2ef6023c436d78a0990a865fdb1fe039dd73197 (commits c579bb332, f2ef6023c), branch agente/21-fv-global-link-small
- mergeHEAD: 7b7b8f1d7135df5f22c8b7450e2a3020ab619db6 (merge of candidate; candidate ancestor verified)
- Review diff sha256 (git diff --binary BASE HEAD): d5b33c8ecf8fd0cfe3212e6a1d95c40e08f21edaa8736d2c969893b49d39a0e2
- Files: tests/addons_legendas_merge.sh (+1/-1), tests/video_android_cacheboost.c (+4/-2). Zero src/ changes.

## Evidence
- Static review: fv-global-link-static1 (nv-revisao medium). Verdict OK (source-only), no P0-P2.
- Runtime verification: fv-global-link-runtime1 (distinct verifier). Red on BASE (link errors; behavioral rc134 with doubles only), green on HEAD, mutation/control confirmed.
- Canonical gate: 5 of 5 passed, 0 errors, 0 fail, 0 skip, 0 divergences (global-link-small-canonical.log).
- Post-merge (env -i, private HOME/XDG/TMPDIR, NUVIO_PROPERTIES=/dev/null, sandbox-exec global-gate-local-only.sb):
  - bash tests/addons_legendas_merge.sh rc 0
  - bash tests/android_speed_room.sh rc 0
  - bash tests/video_android_cacheboost.sh rc 0
  - python3 tools/varredura-i18n.py rc 0

## Limits
- Fixture evidence only. Not a production fix, not a physical TV claim.
- Doubles return NULL/0: fonteKbpsSessao pin (src/video_android.c:872,1027) not exercised here.
- Fake clock advances +20 ms; no real-time or threading behavior proven.
- Full global suite ran on AV candidate `15fbcbf2dcd5a7a8697a36cd9229b3963d4514ab`: wrapper rc 1, incomplete due to ENOSPC. No passing full global gate or BIB/PAG-candidate global run is established; gate remains required and unwaived. See `STATUS.md` for counts and retained evidence.
- No issue closed. Device acceptance unknown.
