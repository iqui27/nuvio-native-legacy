# PR780 EDID pending

Companion on `2ecc6e089cbdd3db7f349c3fcb97cfb0cfac28a4`. An earlier note said the original failed because the harness was missing. That was not a run. This note replaces it.

## What was wrong

`pedirEdid()` stored `edidDts = 0` before the async `getConfigs` reply. `video_bombear()` treated `0` as "this TV has no DTS" and called `iniciarDts()`. `dtsTentou` then stayed set, so a late positive reply could not undo the conversion.

A later edit kept the reply pending and ignored `HLunaServiceCall`'s return (not a documented success bit). It also treated the callback `void*` as a generation stored in `ctx->ud`, and kept two contexts indexed by `g & 1`.

That callback contract is not proven on a TV. Kodi's helper casts the Luna `void*` to `HContext*` before reading `userdata`. If this TV does the same, every reply was dropped, `edidDts` stayed `-1`, and the 2000 ms fallback started the converter on a TV that declares DTS. A third `video_tocar` before any reply reused the first slot and could publish a stale payload. The generation load and the `edidDts` store were also not one publish.

EDID is the TV, not the title. The helper may keep the context pointer. Rewriting that object while a callback can still run is the bug. No second lifetime around it.

## Fix

One static `NvHContext`, initialized once to `{ edidResposta, 0, 0, 0, 1, 0 }`. `pedirEdid()` does not write its fields again.

- Missing `libhelpers` / `HLunaServiceCall`, or no payload reader: store `0` and return. No call. A later open can still try once.
- Otherwise one `getConfigs` per process. Store `-1`, then call. The return value is ignored. A callback that already stored `1` or `0` is not overwritten, because nothing stores `-1` after the call.
- The callback ignores its opaque argument. It does not read `userdata` and does not cast to `HContext*`.
- Another open while that call is outstanding, or after it, does not call again and does not move `edidPedidoMs`.
- No reply: `nv_dts_native_agora()` still uses that first deadline. Later opens fall back after the same 2000 ms, including across a 32-bit wrap. They do not get a new wait.
- A reply, whenever it arrives, is the device answer until the process exits. `ponytail:` no second query if the TV's EDID changes mid-process (HDMI swap, configd edit). Relaunch picks it up. The context is not reused, so a late callback cannot land on a newer request.

`video_bombear()` converts only when that helper returns `0`. Pending stays native. TrueHD is not a DTS track. "Original" stays native. `audioNaoSup` still converts a known DTS track without waiting. webOS older than 5 still does not convert. The Dolby Vision branch does not read EDID.

## Same harness, two sources

`tests/dts_edid_pending_lg.sh` compiles the webOS half of `src/video.c` (the host guard is `NV_DTS_EDID_PENDING_TEST`; a normal Mac build still gets the stubs). The callback is stored and fired only after `video_bombear()`. The clock is `SDL_GetTicks`. Nothing touches `/tmp/nuvio-dts-liberar`.

`git diff 2ecc6e089cbdd3db7f349c3fcb97cfb0cfac28a4 -- src/video.c tests/dts_edid_pending_lg.c` SHA256 `2b934a87e1b5a6eeb6e5dede84f1a9182855a6803b427924d2efcff78c9eee49`.

`git diff 21a11f7fab61314c47943a649a1063efba9e7e7e -- src/video.c tests/dts_edid_pending_lg.c` SHA256 `83a2d538a2eab8c80cbed9a1a12873ea1d9caed29be2147fe202b8be83a0ea93`.

File SHA256: `src/video.c` `2b776c20f8a7993aab96af68fdf242e20455ec49df13b844c3141a497468e7c2`, `tests/dts_edid_pending_lg.c` `11e98ecad1c1f2af0f0e638b948f86660412b3dee730a3a148dcd8ac1243e1cf`.

Observed on this tree (`TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_edid_pending_lg.sh`):

```
dts_edid_pending_lg ok
rc=0
```

Original `src/video.c` from `2ecc6e089cbdd3db7f349c3fcb97cfb0cfac28a4`, same test file, same stubs. Scratch copy at `/Volumes/ExternalSSD/tmp/pr780-oneshot-orig/video.c`. The only edits in that copy are the host `#if` (`NV_DTS_EDID_PENDING_TEST` skips the Apple stubs and can force `webosMaior`) plus the `nv_dts_edid_test_webos_major` definition those lines need. No predicate was copied out of `video.c`. Built with `-DNV_EDID_ORIGINAL` so `expWin` is set and the test does not name symbols this fix added.

```
[video] starfish-release: (sem arquivo)
[video] webos major=0 fonte=-
FAIL tests/dts_edid_pending_lg.c:232 !dtsTentou && !dtsSessao
rc=1
```

Line 232 is the pump while the callback is still held. `major=0` is the raw `nv_webos_major()` log; `expWin` then makes `webosMaior()` return 5, which is why conversion ran. This tree does not convert there, then accepts the late positive reply and still does not convert.

Also covered here: helper library absent, symbol absent, payload reader absent, helper return 0 with no callback, return 0 after a synchronous positive callback, timeout at 2000 ms, timeout across a 32-bit wrap, timeout from tick 0, negative / malformed / error JSON, Original, unsupported-audio bypass, TrueHD, three opens with the callback given the context pointer (not `ctx->ud`), timeout then a later open that does not call again, webOS 4.

`tests/dts_tv.sh` checks the unsigned edges without sleeping: zero, 49/50, wrap `0xfffffff0`, and `agora < desde`.

## Not this change

An earlier source candidate failed `tests/ajustes_dts.py` on an outdated catalog assertion. The integration candidate updates that test to the current platform-specific DTS settings and the executor reports it and `tests/ajustes_dts_tpk.py` passing. These checks do not exercise `pedirEdid`.

## Checks

- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_edid_pending_lg.sh` → rc 0
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tests/dts_tv.sh` → rc 0
- `TMPDIR=/Volumes/ExternalSSD/tmp bash tools/build-local.sh` → rc 0

Host build compiles the Apple `video_tocar` stub, not `pedirEdid`. No TV, no `libhelpers.so.2`. The opaque-pointer case is the host fake, not a Luna capture.

## Limits

One query per process. No callback means every later open uses the first deadline, then the converter, until relaunch. A device-wide reply after that fallback does not undo a conversion already started; later opens see the cached 0 or 1. EDID is not refreshed while the process lives.
