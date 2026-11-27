# Source-list performance port

> On `feat/formatter-icons` these commits sit on top of the badge commits
> (imported badge styles, long badge rules, one-pack readiness and the From
> addon badge row with its position setting), so the badge items listed as
> unchanged below do change on this branch, in those earlier commits. The
> off-screen line skip is in `addonDesenhar`, and the badge-worker checks
> live in `tests/source_badge_readiness.py` instead of a separate
> `source_badge_worker.py`.

Base: `iqui27/nuvio-native-legacy` master at
`7cac4f25d11931b56c41cfefb952e4e183c45c1a`.

This branch ports only applicable source-list performance work from
`Nuvio_webOS_2.0.8.6`, without importing that branch's other features or history.

- Merge arriving addon batches in linear time while preserving stable ordering.
- Use a stable merge sort for source sizes and reuse existing scratch storage.
- Skip impossible release-scoring token comparisons and cache the automatic
  candidate across display-only tab/chip changes.
- Cache declared audio-language filter results, retaining upstream's strict
  declared-audio semantics. Appends retain existing entries; replacement,
  renewed source metadata, language changes and list cleanup invalidate them.
- Copy one badge-worker source at a time instead of duplicating a whole batch
  on the UI thread. Cancel stale list/pack jobs before publishing their result.
- Skip addon description lines outside the scissor without changing layout.
- Reuse the existing RGB snapshot for a settled static webOS title background.
  Invalidate it for metadata, profile, title, theme, scale and snapshot ownership
  changes. Keep video, animated themes, nested captures and transitions live.
- Let source selection before video assignment follow webOS VSync. Preserve
  existing pacing for assigned/retained video and other platforms, with a
  bounded fallback when enabling VSync fails.

The rich formatter's dynamic-height tree and pixel-grid changes are not
applicable to upstream's fixed-line formatter and are not imported. Badge-pack
compatibility, badge-position settings, profile memory, audio-track memory,
skip controls, source-request deduplication, playback engines, network buffers,
assets, version labels and CI workflows are unchanged. No detailed performance
diagnostics or temporary fast-build exceptions are added.

## Validation

```sh
bash tests/source_performance.sh
SANITIZE=1 bash tests/source_performance.sh
bash tests/selospacote.sh
bash tests/fontes_formatter.sh
```

The focused tests extract and execute actual production functions, comparing
ordering, scoring, language matching and clipped drawing against the original
algorithms. They cover cache invalidation, worker lifecycle and pack/list
replacement, five-minute backdrop reuse, late artwork, all VSync transitions,
and the unchanged pacing policy on Linux, Android, Tizen, Apple and WebAssembly.

The formatter test draws the real source sheet and compares actual GLES pixels,
including the background snapshot beneath moving foreground cards. Linux needs
SDL2, SDL2_image, SDL2_ttf, GLESv2 development packages and a working GL context;
headless Mesa can use `SDL_VIDEODRIVER=offscreen EGL_PLATFORM=surfaceless`.

This port passed a full Linux host application compile/link, the actual GLES
formatter/snapshot checks and focused ASan/UBSan tests. Hardware behavior on LG
must still be confirmed for this upstream-based branch.
