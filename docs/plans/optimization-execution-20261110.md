# Production optimization candidate — 2026-11-10

Status: implemented and committed in an isolated SSD worktree; independent review and TV acceptance pending. Owner authorization: complete audit-plan execution, 2026-11-10. This report covers one bounded optimization, outside Glass/Frost, GPU policy, shared video/ASS and PR729.

## Identity and scope

- Worktree: `/Volumes/ExternalSSD/nv-21-webp-output-20261110`.
- Ref: `refs/heads/agente/21-webp-output-20261110`.
- Base: `d8a403b713a38cc0f9d63e816fc1e016627f72c2` (current integration when created).
- Implementation: `f884187f6236e53295075a28bbfa889a82712920`; report-only commit follows it.
- Production diff: `src/webp.c`, 14 additions / 13 deletions. Tests: `tests/webp-output.c`, `tests/webp-output.sh`. No other production file changed.
- Prior research inspected: `claude/central-otimizacao-ced884` at `8f6609ec61d9fc2c009cb96292cafb9149de2179`, especially `docs/optimization/{README,webp-host,continuation-audit}.md` and the reviewed copy harness. No production WebP writer found in current job ledger/recent all-branch file history. TV and Android activity notes read; their active files are untouched.

Gap: successful native scaled WebP loading allocated decoder-owned output, copied it into an owned RGBA buffer, then copied that into SDL. The current production callers are `tex_cache.c`'s memory and file artwork loaders → `webp_carregar_larg_mem` → `decodificarEscalado`.

Change: set the existing advanced decoder's external-memory output to the owned contiguous RGBA buffer. Keep dimensions, scaling options, final SDL copy, original metadata, animated/unscaled paths, ABI refusal and full-size fallback. No new API symbol, dependency, visual/default/layout/animation change.

## Measured mechanism

The target check includes actual production `src/webp.c`, observes its resolved decoder and counts its `memcpy` bytes. Production and the independent official-header reference resolve the same Homebrew library. Baseline input is the exact base Git object, not copied research runtime sources.

For successful 3840×2160 → 1920×1080 loading, observed output-copy bytes are **16,588,800 → 8,294,400**; decoder external-memory state is **0 → 1**. The same two-to-one output-transfer reduction holds for all five scaled fixtures. Unscaled stays one transfer. Source ownership semantics eliminate one decoder-owned full-output allocation (8,294,400 bytes in that case). Decoder-internal scratch allocations are excluded.

**Overall loader peak remains two output buffers:** the owned RGBA buffer and SDL still coexist. This is not the research harness's more invasive zero-copy-to-SDL candidate and does not claim its 50% peak saving. No RSS/PSS reduction or TV improvement measured.

## Bounded timing

Host: Darwin 27.2 arm64, Apple clang 21.0.0, `-O2 -g -Wall -Wextra -pthread`, libwebp 1.6.0, SDL 2.32.74. Synthetic deterministic RGBA encoded once per case/run; no disk, network, app or device. Same test source/compiler/dependencies for both binaries, with baseline production source selected by `WEBP_OUTPUT_SOURCE`. Transfer-count instrumentation is present in both and adds row-counter overhead; these are instrumented loader timings, not whole-app timings.

Three sequential rounds alternate process order B/C, C/B, B/C. Each case has two warmups then seven timed calls per round; `CLOCK_MONOTONIC`, encode/parity/free excluded. Median is nearest-rank; nearest-rank p95 at n=7 is the maximum. Cases/rounds are not pooled. Acceptance is exact pixel/fallback parity plus removal of one transfer; timing has no invented pass threshold.

Values below are the three per-round medians in ms. Raw per-call samples and per-run p95/min/max are preserved in the evidence directory.

| Fixture (source → requested width) | Baseline medians | Candidate medians |
|---|---|---|
| lossy-4k (3840×2160 → 1920) | 48.881 / 48.737 / 48.967 | 48.380 / 48.750 / 48.488 |
| lossless (1920×1080 → 960) | 15.081 / 14.945 / 15.764 | 14.829 / 14.987 / 15.080 |
| alpha (1280×720 → 640) | 6.861 / 6.793 / 6.705 | 6.804 / 6.688 / 6.720 |
| odd (1001×501 → 301) | 3.551 / 3.566 / 3.568 | 3.498 / 3.715 / 3.564 |
| one-row (71×1 → 35) | 0.020 / 0.019 / 0.021 | 0.019 / 0.020 / 0.019 |
| unscaled (64×64, limit 320) | 0.033 / 0.032 / 0.033 | 0.032 / 0.033 / 0.033 |

4K p95/max B: 49.138 / 49.843 / 52.089 ms; C: 49.239 / 50.118 / 48.624 ms. Ranges overlap. **No repeatable CPU speed improvement beyond noise established.** The supported benefit is eliminated output-allocation/copy work, not an FPS claim.

## Checks and limits

All commands ran in this worktree with `TMPDIR=/Volumes/ExternalSSD/tmp` where applicable.

| Check | Result |
|---|---|
| Baseline `EXPECT_TRANSFERS=2` target harness | rc 0; six cases, exact row/pitch-aware RGBA parity, original dimensions, truncation and four fallback modes |
| Baseline binary `EXPECT_TRANSFERS=1` | rc 134 at copied-output assertion (red); this is a structural transfer contract, not a correctness defect or timing threshold |
| `bash tests/webp-output.sh` | rc 0 (green); six cases; five scaled external-output paths; no advanced decode on unscaled case |
| Three alternating baseline/candidate benchmark rounds | rc 0 for all six processes; 7 samples/case/process |
| `bash tests/webp.sh` | rc 0; includes eight-thread concurrent loading, VP8L, alpha and animated first-frame fixtures |
| `cc -fsyntax-only ... tests/webp-output.c` against cached libwebp 1.4.0 headers | rc 0; six host struct size/offset assertions; not ARM or firmware runtime proof |
| `timeout 45 env TMPDIR=... SANITIZE=1 bash tests/webp-output.sh` | rc 124; zero-byte log, no test output. Sanitizer verification incomplete; cause unproven |
| `TMPDIR=... timeout 180 bash tools/build-local.sh` | rc 0, `build: ok`; one compiler warning in log. Began only after internal free space recovered to 11 GiB (floor 5 GiB) |
| `git diff --check` | rc 0 |

Missing/refused advanced API, injected decoder error and owned-output malloc failure all return full-size fallback with exact reference pixels. Actual truncated inputs return NULL. Existing native `soltar` allocator assumption remains unchanged. Cached libwebp 1.4.0 `buffer_dec.c:96–124,238–244` confirms external output skips its output allocation and is not freed by `WebPFreeDecBuffer`; installed old-TV ABI/runtime behavior remains untested. WGT browser route is unchanged and receives no optimization claim.

Evidence: `/Volumes/ExternalSSD/tmp/rev/optimization-webp-output-20261110/` contains frozen binaries, `webp-base.c`, `baseline-round{1,2,3}.log`, `candidate-round{1,2,3}.log`, `summary.json`, `red.log`, `green.log`, `webp.log`, `sanitize.log`, `build-local.log`.

SHA256: baseline source `9419175e53ddaa6714f8e4d0e0ab282994dc467ab4cf26965f58c336e2161959`; candidate source `a91262bd0ad8f3ea67cc9f23f9501f886358e38793e729b250ba166932c0c83c`; baseline binary `4a945b94cc7fe7e08df4eb98fb23434b35f7d12352e715c698a85c3b52058e98`; candidate binary `0c2775ccf157887d34d0dffe6c09e5f6e9f83e21dfbea0ad7c062201a0a75a91`; summary `e24841029a4de0023b8a18c96cd961d9561f25a35f929da8a1e44255cf05cae8`.

Reviewer: none in this execution; parent dispatches independent review of the frozen base/head before integration. Parent owns integration validation entries in `docs/issues/mapa.json`. No device action, install, account write, settings/provider change, push, deployment or integration merge performed. Release symbols/data/secrets preserved.
