# PR780 EDID pending

Companion on `2ecc6e089cbdd3db7f349c3fcb97cfb0cfac28a4`. An earlier note said the original failed because the harness was missing. That was not a run. This note replaces it.

## What was wrong

`pedirEdid()` stored `edidDts = 0` before the async `getConfigs` reply. `video_bombear()` treated `0` as "this TV has no DTS" and called `iniciarDts()`. `dtsTentou` then stayed set, so a late positive reply could not undo the conversion.

A later edit kept the reply pending, but treated `!HLunaServiceCall(...)` as failure. That return value is not a documented success bit. A tick wrap (`agora < desde`) also left the wait pending forever.

## Fix

- Missing `libhelpers` / `HLunaServiceCall`, or no payload reader: store `0` and return. No call.
- Otherwise store `-1`, then call. The return value is ignored. A callback that already stored `1` or `0` is not overwritten.
- Two context slots and a generation counter. A late callback from the previous request does not change the new one.
- `nv_dts_native_agora()` uses unsigned elapsed. A 32-bit wrap stays inside the 2000 ms wait. Zero wait expires immediately.

`video_bombear()` converts only when that helper returns `0`. Pending stays native. TrueHD is not a DTS track. "Original" stays native. `audioNaoSup` still converts a known DTS track without waiting. webOS older than 5 still does not convert.

## Same harness, two sources

`tests/dts_edid_pending_lg.sh` compiles the webOS half of `src/video.c` (the host guard is `NV_DTS_EDID_PENDING_TEST`; a normal Mac build still gets the stubs). The callback is stored and fired only after `video_bombear()`. The clock is `SDL_GetTicks`. Nothing touches `/tmp/nuvio-dts-liberar`.

Observed on the corrected tree (`TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_edid_pending_lg.sh`):

```
dts_edid_pending_lg ok
rc=0
```

Original `src/video.c` from `2ecc6e089cbdd3db7f349c3fcb97cfb0cfac28a4`, same test file, same stubs. The only edit in that scratch copy is the same host `#if`, so the Mac compiler enters the webOS body. No predicate was copied out of `video.c`.

```
[video] starfish-release: (sem arquivo)
[video] webos major=0 fonte=-
FAIL tests/dts_edid_pending_lg.c:229 !dtsTentou && !dtsSessao
rc=1
```

Line 229 is the pump while the callback is still held. `major=0` is the raw `nv_webos_major()` log; `expWin` then makes `webosMaior()` return 5, which is why conversion ran. The corrected binary does not convert there, then accepts the late positive reply and still does not convert.

Also covered on the corrected binary: helper library absent, symbol absent, payload reader absent, helper return 0 with no callback, return 0 after a synchronous positive callback, timeout at 2000 ms, timeout across a 32-bit wrap, timeout from tick 0, negative / malformed / error JSON, Original, unsupported-audio bypass, TrueHD, stale callback, webOS 4.

`tests/dts_tv.sh` checks the unsigned edges without sleeping: zero, 49/50, wrap `0xfffffff0`, and `agora < desde`.

## Checks

- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_edid_pending_lg.sh` → rc 0
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_tv.sh` → rc 0
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tools/build-local.sh` → rc 0 (`build: ok`). Disk at the run: internal 8.4 GiB free, ExternalSSD 38 GiB free. Floor is 5 GiB.

`python3 tests/ajustes_dts.py` is not a check for this change. It still exits 1 at `assert 'DTS' not in catalog` (`AssertionError: None`). `python3 tests/ajustes_dts_tpk.py` exits 0 and does not exercise this path.
