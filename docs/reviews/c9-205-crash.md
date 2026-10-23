# LG C9 movie-open crash investigation

Crash cause remains unproven. This change fixes lost crash evidence and adds bounded thread-creation diagnostics; it does not claim to fix playback or disable the previously working Dolby Vision path.

## Evidence and execution order

Inputs: `/Volumes/ExternalSSD/tmp/rev/c9crash/{nuvio-anterior.log,nuvio.log,nuvio-marcos.txt}`. No network or TV access used. Worktree started at `2c98dd2e8` (documentation-only change after installed integration `e5fc8f342`).

The failing session reports webOS major 4 from nyx, explicitly disables DTS conversion, then opens the separate Dolby Vision MKV path: HEVC 3840x2160 profile 8, E-AC-3 six channels, stream 1. Native Load and both initial feeds succeed. The last drained adapter event is `native-play-accepted count=1`, immediately followed by the fatal-signal message. Both saved crashes have fault address `0x8a54`, with relocated PCs `0xe51562a2` and `0xe7f572a2`.

The adapter's `play()` (`src/dts/adapter/starfish.cpp:244`) calls firmware Play and queues the accepted-stage event. These events are queued through `nativeEvent()` (`src/dts/dts_playback.c:91`) and later printed by `logDtsStage()` (`src/video.c:3010`); their log position does not identify the instruction or thread that faulted. After Play, the worker continues demux/feed, pacing and native callbacks. LG GStreamer audio/video startup runs concurrently.

Other nearby work:

- `src/faixas.c:1128`: selects remembered Portuguese addon subtitle 21; `escolherLegenda()` hands it to `legsync_primaria_externa()` (`src/faixas.c:865`), disabling embedded subtitles and loading the external document. The logged sync state (`src/legsync.c:692`) still says subtitle downloading, no reference yet.
- `src/dvtela.c:165`: logs the opening screen's transition to waiting for image. This is UI state, not proof of a presented native frame.
- `src/player.c:2316`: logs MKV credit chapter selection at 9691/9923 seconds; ordinary credit selection, not an end-of-film action.
- Firmware prints `KADP_AUDIO_OpenMaster` and GStreamer critical messages about disposing parented registry objects (`dvbin`, `lxvideosink`, `ac3_audiodec`, `audiosink`). No shutdown or source switch is logged after native Play.

## Ranked hypotheses

1. LG audio startup repeats the previously diagnosed C9 thread-creation failure. `src/dts/rt_shim.c:4` documents a prior core: `pthread_setname_np` called after `KADP_OSA_CreateThread` mishandled positive `EINVAL`, with SCHED_RR priority 60 and a 24576-byte stack. Same model, firmware generation, timing and repeated small address support this hypothesis. But the existing shim already retries with default attributes, and this session has no retry message. The supplied files contain neither a raw core nor complete module maps, so this is not a symbolized diagnosis of these crashes.
2. Firmware GStreamer registry/object lifetime corruption, possibly during URI-to-BUFFERSTREAM handoff or native callbacks. The parent/refcount warnings support investigation; they do not prove a stale callback. Repeated relocated PC low bits also fit a stable instruction in a mapped library. An unmapped function pointer cannot be inferred from the current log.

Next run gains the actual fault module/offset, LR, crashing thread ID/name, bounded stack candidates, shim activation and original thread attributes/error before retry, plus retry result. Those separate a thread-creation failure from a firmware object/callback fault without packet, URL or frame logging.

## Proven bug and minimal change

The original webOS signal path copied only the first 24 KB of `/proc/self/maps`, then the reader accepted only 512 map entries. Late-loaded audio libraries could disappear from the report and falsely produce `fora de qualquer modulo`. Origin: `48d49affe` (October 5), not the recent merges.

`src/queda.c:62` now reuses the existing Android capture routine, scanning all maps and retaining only fault/LR/stack-relevant mappings and module bases. It also records thread identity and bounds stack reads by their mapping. The report mirror and normal signal disposition remain. `src/dts/rt_shim.c` adds one activation line per process and at most four failure/retry diagnostic pairs, using an atomic log budget. Retry behavior is unchanged.

`tests/c9_queda.sh` exercises the actual webOS handler with 700 earlier map entries and the supplied PC/address shape: before the fix it aborts (rc 134), missing the fault module; afterward it resolves PC/LR and thread identity. It also stubs EINVAL and verifies the existing shim retries successfully, preserves detached state and emits diagnostics. It does not reproduce the TV playback crash.

## Regression review and validation

Reviewed integration history for the preceding 48 hours and playback-related diffs. No changes to Starfish adapter, thread shim, crash recorder, legsync core or dvtela in that window. `3c3aa2f2f` adds Samsung converted-audio offset, outside the LG native path. `fc94e9c24` changes SRT tag parsing; subtitle content is absent, so it cannot be conclusively excluded. Home/library/live-guide merges have no demonstrated causal link. The DV route originated in `80c60e1c7`, whose commit records owner-confirmed C9 DV picture and E-AC-3 sound with this shim. No crash regression commit established. Repository is shallow; no history fetched.

Final checks: target test, existing `tests/queda.sh`, `tests/i18n.sh`, `tools/build-local.sh`, and ARM `tools/arm.sh --build` all rc 0. Docker build used `--network none --pull never` with the locally cached SDK. Both native adapters and full ARM binary built; readelf confirms ELF32 ARM and a global exported `pthread_create`. Build has existing compiler/linker warnings and empty local service credentials; this is a verification binary. Nothing pushed, deployed or installed.

Pending: a future authorized TV run and its new crash report/core are required to prove and fix the actual movie-open crash.
