# PR780 EDID pending regression

## Reviewed range
- Base: `7cac4f25d11931b56c41cfefb952e4e183c45c1a`
- Head (companion fix work): `79da20ae47d7af3128abcf9a6e0b8873b4111337`
- `git diff BASE HEAD` SHA256: `8537a9b60c1bd8befdc32500c652eab5b8786dd69c0c8c2dc36ed8601e833bfb`

## P1 before this fix
On webOS 5+ with converted DTS output enabled, `pedirEdid()` stored `edidDts = 0` before the async config reply. `video_bombear()` treated that as "no DTS" and called `iniciarDts()` once a DTS track was known. `dtsTentou = 1` latched the software path, so a late positive EDID reply could not switch the source back to native playback.

Predicate on the original head (without tri-state decision):
- delayed positive EDID reply: commit early = `1`, undo = `0` → fail
- pending state: none, only `0/1`

## Minimal fix
- `edidDts` is tri-state: `-1` pending, `0` answered negative, `1` answered positive.
- `nv_dts_native_agora()` in `src/dts_tv.h` returns convert / native / wait.
- `video_bombear()` converts only when native decision is `0`, not while still pending.
- `pedirEdid()` marks missing helper / failed call as answered negative (`0`) to preserve the old safe fallback.
- Bounded wait: 2 s, then fall back to convert if the reply never arrives.

## Regression test
`tests/dts_edid_pending_lg.c` compiles the real `src/video.c` path on the host and fakes only the EDID helper callback.

Scenarios covered:
1. EDID still pending when a DTS track appears → no `iniciarDts()`.
2. Delayed positive reply → stays native.
3. Negative reply → converts.
4. Missing helper/call failure → immediate negative, converts.

Timeout-with-no-reply is covered at the predicate level in `tests/dts_tv.c` (`nv_dts_native_agora()` after the wait window) and in production `edidEsperaMs` handling.

The head commit without this companion fix failed scenario (1). With the companion fix, the host harness passes scenarios 1–4.

## Required checks (implementation worktree)
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_edid_pending_lg.sh`
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_tv.sh`
- `python3 tests/ajustes_dts_tpk.py`
- `python3 tests/ajustes_dts.py` still fails at the pre-existing `OPC(AJ_DTS_AC3)` catalog assertion (baseline, not introduced here).

Build note: full `tools/build-local.sh` not run in this job because host disk headroom was below the central threshold at dispatch (~42 GiB free on `/Volumes/ExternalSSD`, internal ~5 GiB).
