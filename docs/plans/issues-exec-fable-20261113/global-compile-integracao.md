# Global compile fixture cut — integration (fixture-only)

Scope: test fixture repair only. No `src/` change. No product fix. No issue closed. Not a global-gate pass.

## Merge
- Candidate branch `agente/21-fv-global-compile`, head `fa32c8e68d7eeb92127d82179ea7958f01a9959d`.
- Prior integration head `6ac09088b04123a4b29e45901e253a652477f968` (first parent).
- Merge commit `9ddae314efae99413f95f236ebcc7f5fa7f253a0` (ledger `merge` event, merge_rc 0).
- Review diff sha256 `120b888ffc8fa2e88c37a00566c621dbcc69b641e48f5e812870b05eeca1d03a` (7 files under `tests/`, +35/-11).
- Verification digest (ledger) `900744a7c1122ce1b4c695a9e3f213d25270c4ad75f2e7a9d43effdbda486124`; ledger postcheck passed rc 0.
- Review verdict OK (static reviewer `fv-global-col-static1`, runtime verifier `fv-global-col-runtime1`). No confirmed P0/P1.

## What changed
- 7 test files: 4 compile failures repaired (`tests/colestilos.c` and related harnesses).
- Poster G geometry expectation matches the Oct 17 contract (Large poster widening after height cap).
- Shot hooks replace blob-based setup; `-DNV_SHOT_HOOKS` required (already needed by `tests/cwordem_home.c`).

## Evidence (post-merge, this session, 2026-11-13)
Run from `/Volumes/ExternalSSD/nv-21-int` under `env -i` with private HOME/XDG/TMPDIR on the SSD, `NUVIO_PROPERTIES=/dev/null`, `sandbox-exec -f global-gate-local-only.sb`.

| command | rc | log | sha256 (prefix) |
|---|---|---|---|
| `bash tests/colestilos.sh` | 0 | `global-compile-postmerge-colestilos.log` | 2870603f… |
| `bash tests/colordem.sh` | 0 | `global-compile-postmerge-colordem.log` | cf645c64… |
| `python3 tools/varredura-i18n.py` | 0 | `global-compile-postmerge-i18n.log` | e2dc1d85… |

- First `colordem.sh` attempt failed rc 1: `pkg-config: command not found` under the sanitized PATH (`SDL2/SDL.h` not found). Log kept as `global-compile-postmerge-colordem-attempt1-pathfail.log` (sha256 dc6a018a…). Fixed only the environment (added `/opt/homebrew/bin` to PATH); no test or source change. The rerun is the evidence above.
- Canonical ledger: merge event recorded `status: passed`. The per-check 6/6 count (zero fail/skip/divergence) is taken from the ledger as recorded; the separate `central.py verifica` replay was not re-run by this job.

## Limits (unchanged, not waived)
- F1 (P2, unverified): Padrão (classic) collection vs catalog width parity not exercised; fixture covers Moderna only. Follow-up triage needed.
- F2 (P2, confirmed coverage gap): `tests/colordem_menu.py` pins `palcoLinha=0`; real collection menu has the Stage row (`ctxmenu.c:798`, `fileiras.c:199`). Not covered.
- Overall AV/BIB global gate remains REQUIRED and unwaived. No global-pass claim.
- No TV source fix claimed. No issue closure. Issues #639 and #789 receive fixture/infrastructure evidence only.
