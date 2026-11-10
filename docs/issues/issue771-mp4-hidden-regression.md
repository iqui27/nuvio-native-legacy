# Issue 771 — MP4 hint probe regression

Parent `ca8c9fc772b434ae09fc750b35ba2bce183759aa` on base `63e808aa876b091810845d2d9b7945668d069490`. No stash. Not an acceptance.

## Cause

`sondaMkv` returned on cache miss, so Range never ran. `mkvPrecisaCabecalho` was true for every recon probe (`mkvEstado` 1/2), so a real MP4 with no audio never side-probed the converter and `video_mkv_sondado` stayed 0.

## FAIL before the source change

Same new `tests/tpk-mp4-mkvfix-legendas.c` linked to `git show ca8c9fc:src/video_tpk.c` (copy `/Volumes/ExternalSSD/tmp/rev771-ca8-video_tpk.c`), not the worktree object. rc **1**, 9 FAIL.

- B hidden false MP4, header only by Range: `rede=0 nLeg=0 sondado=0`
- C ordinary MKV, no prefetch: `rede=0 codec=- sondado=0`
- D true MP4, no hidden, no cache, 0 audio: `redeDelta=0 sondado=0 dtsDelta=0` (no Range held; probe and converter fail)
- E correct hint: `sondado=0` (no Range held)

## PASS after

`TMPDIR=/Volumes/ExternalSSD/tmp bash tests/tpk-mp4-mkvfix-legendas.sh` rc **0**.

- A cached hidden text, no PGS: 2 file subs, ASS, ordinals 0 then 1, overlay, `rede=0`
- B: `rede=1 nLeg=2 sondado=1` ASS
- C: `rede=1 codec=S_TEXT/ASS sondado=1`
- D: `redeDelta=0 sondado=2 dtsDelta=1`
- E correct hint, TV audio, no hidden: `redeDelta=0 sondado=2 dtsDelta=0`
- F chosen ordinal 1 kept, host not rewritten
- G live, recon off: no Range, no converter open

## Baseline (separate source, not stash)

`/Volumes/ExternalSSD/tmp/rev771-review/harness-base` built from `video_tpk_base.c`. rc 1. C `rede=1` ASS `sondado=1`. D `redeDelta=0 sondado=2 dtsDelta=1`. B `rede=0 sondado=2` (MP4 hint never Range). A cached menu empty. F ordinal not kept.

## Adjacent and build

`tests/tpk-dts-legendas.sh` did not link on ca8 or on base (`video_taxa_efetiva`, audio/hdr/debrid/dts normalize, ling helpers). Link now uses `src/video_modo.c` plus local stubs. Assertions still **11 FAIL**, same on the ca8 `video_tpk.c` object and on this tree. Inherited, not this probe change.

`TMPDIR=/Volumes/ExternalSSD/tmp bash tools/build-local.sh` rc **0** (one deprecated `ATOMIC_VAR_INIT` warning).
