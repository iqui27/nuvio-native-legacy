# ready-tv lot — integration record (source only)

Status: source merged into `integracao/2.1`. NOT TV acceptance. Physical TV acceptance unknown.

## Provenance
- Integration worktree: `/Volumes/ExternalSSD/nv-21-int`, branch `integracao/2.1`.
- Merge commit: `c5548b46578a02d69c541e8043e874ae8a3e8d18`.
- Candidate: `5f1898a2c8196fb62cca835199b0ce7c4434e4de`.
- Base: `21e1565df9b6cd4112c4cabc9f7ab2f26eef295d`.
- Diff `git diff BASE HEAD` SHA-256: `b046cffc816b5f2881c17413f83c17a8c62b244d2232c81122dcd44810a05726`.
- Verification digest: `896288673a9acae814aae0e2060d7bb79f20ccc57ab62c18f382d2ca95bf97e1`.
- Integrator i18n postcheck rc 0, SHA `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1`.
- Diff scope: 16 files, +552/-16. Candidate files byte-identical to HEAD.

## Items in this merge
- DMP766: crash dump storage hygiene (`src/queda.c`, `src/arranque.c`). Not the crash root cause. Candidate: `254fba22bdfb40126c0bb31efcfe9ec5723c2fa2`.
- SAM / P2P783: VOD Home-return mechanism (`src/video_tpk.c`, Tizen `Video.cs`). Candidates: SAM `9751393737a86eca5706a2f797a5dc592ad751b8`.
- ASS719: active line and same-centisecond sibling retention (`src/mkvass.c`). Candidate: `fe310be343b52b7dda80bbc300b915e2803f5073`.

## Review and verification (source only)
- Static reviewer: `fv-ready-tv-static-review` (served model UNKNOWN). Verdict OK: no confirmed P0/P1. Report: `ready-tv-code-review.md`.
- Runtime verifier: `fv-ready-tv-runtime-verifier` (served model UNKNOWN), attempt `resume-20261113`. Verdict OK on host gate. Report: `ready-tv-verification.md`.
- Host gate: 7 declared commands, 7 executed, 7 passed, zero failures, timeouts, divergences, errors.
- Note: the reviewer's "OK" is a source conclusion, not merge approval. Merge was done by the central under the guarded flow.

## Post-merge tests (this job, on c5548b4, sequential)
- `TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp/fv-ready-tv-postmerge NUVIO_PROPERTIES=/dev/null bash tests/queda.sh`: rc 0, log SHA-256 `1b8fdbaab657f5473aca1fc238c1e36086bb25a48c3b8cee827bfc689c1742bb`.
- `... bash tests/tpk-preso.sh`: rc 0, log SHA-256 `901d16386671740c614436fc7b5faa034d85887a3c63d15628cf8a71b31d7475`.
- `... bash tests/mkvass_despejo719.sh`: rc 0, log SHA-256 `a5a39abba3d6f425bb5b72e97cf884fe9b51ec536c78df7987294efec8259afd`.
- Raw results: `/Volumes/ExternalSSD/tmp/jobtmp/fv-ready-tv-postmerge/results.json`.
- No rebuild and no re-run of the seven-command gate.

## Known limits and open items
- Physical TV acceptance unknown for all three items: webOS core prune on a TV (DMP766), Tizen Home-return resume (SAM/P2P783), ASS seek-back after a despejo on an anime track (ASS719).
- P2 notes retained, not blockers:
  - SAM F1: `Video.cs:299` / `:316` sends `posMs` (>=0), bypassing the C fallback at `video_tpk.c:1591`.
  - SAM F2: early returns in `video_buscar` (`video_tpk.c:2002/2006/2012`) do not clear `playerPreso`.
  - SAM F3: `RetomarDoSistema` emits EV_PRESO outside `trocaTrava` (`Video.cs:316`).
  - SAM F4 and F5: host red only shows missing API; stash provenance unverified.
  - ASS F4: rel<0 cluster points can duplicate a line on short seek-back.
  - ASS F5: no byte cap on active/held lines.
  - ASS F6: repeated parsing CPU cost, unmeasured.
  - DMP F7: `src/queda.c` `openat` lacks `O_NONBLOCK` and fstat recheck (same-uid FIFO race).
- BIB639 / PAG789 full global gate: still pending. Not waived, not integrated.
- DMP766 / SAM783 / ASS719 issue closure and item status: unchanged. Issues stay open.
