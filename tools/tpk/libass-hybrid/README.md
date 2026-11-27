# libass hybrid (test branch `libass-hybrid-test` only)

This branch tests one question: is **Laskco/mpvNova-libass + our libass-nv
patches** a better libass for the .tpk than what ships today
(0.17.5 + `tools/tpk/libass-nv/patches`)? mpvNova-libass is the libass that
NuvioMedia/NuvioTV switched to in PR 4799 (nuvio-libass v0.0.14).

The branch is `subtitle-rewrite` (f3963200) plus this folder. Burn-in, sync,
the Stremio look, the split render and everything else are unchanged. Only
the libass source changes.

## What is in here

| File | What it is |
|---|---|
| `hybrid.patch` | Applies to mpvNova-libass `b620927` (= libass 0.17.5 + 29 upstream fixes + 61 Laskco commits). It carries our 0001-0004 ported onto that tree, plus two hybrid fixes (below). |
| `cflags` | `-O3 -mtune=cortex-a53 -DASS_EVENT_THREADS` (the define turns on mpvNova's event threads). |
| `bench/` | `bench.c` (times `ass_render_frame` on a 24 fps grid at 1080p and hashes every frame), `gen_heavy.py` / `gen_fx.py` (synthetic stress tracks), `run.sh` (builds stock / ours / nova / hybrid and runs everything). |

`tools/tpk.sh` builds the hybrid by default on this branch (`NV_ASS_BASE=hybrid`).
`NV_ASS_BASE=nv` builds the old 0.17.5 + libass-nv instead, for an A/B .tpk.
The .tpk's `[ass] tv:` log line ends with `libass=mpvNova-libass b620927 + libass-nv`
when the hybrid is inside.

## How the four pieces fit

| Piece | Stock 0.17.5 | Ours (libass-nv) | mpvNova | Hybrid |
|---|---|---|---|---|
| armv7 bitmap engine | plain C | neon32 (0001), bit-exact | its own NEON intrinsics, bit-exact | mpvNova's (`-DASS_NV_ARMV7_NOVA=1` in `cflags`: it was faster on the real ARM core); `ASS_NV_ARMV7=nv` picks ours |
| Blur threads | no | 0002: stripes over 1-4 helpers (app: 3) | vertical blur passes split over its pool | both (our helpers back off when busy: trylock) |
| Event threads (several lines at once) | no | no | up to 4 per renderer | yes |
| Clip culling (crop a glyph to its `\clip`) | no | no | yes | yes, with the moving-clip guard |
| App API (`ass_nv_*`: engine, threads, dialogue opacity, split render, `nv_sign`) | no | 0003 + 0004 | no | yes, also on the event workers |
| "Performance mode" `LIBASS_EXPERIMENTAL_EDGES=1` (NuvioTV's default) | - | - | changes pixels | off (we keep bit-exact output) |

### Hybrid fixes on top of a plain merge
1. **Worker settings.** mpvNova renders events on separate worker renderers.
   Our per-renderer settings (bitmap engine, dialogue opacity, split pass,
   dialogue skip) are copied to every worker each frame. Without this, dialogue
   opacity and the split render would silently stop working when threads kick in.
2. **Moving-clip guard.** mpvNova puts the clip crop window into the bitmap
   cache key. When a clip moves every frame over the same drawing (wipes,
   reveals, frame-by-frame clipped signs), every frame misses the cache and the
   blur is redone: a blurred full-screen drawing under a moving clip took
   **3x as long as stock**. The hybrid remembers each outline's last crop
   window and draws it uncropped once the window changes (then cached, as stock).
   Same bytes. `ASS_NV_CLIP_CROP=always` turns the guard off.

## Checks (all run, 25/11/2026)

- `verify.c` (C vs NEON with 1 and 4 blur threads, dialogue opacity, split
  sharp+soft = normal, signs): **passes** on the hybrid on armv7 (both engines,
  event threads 1 and 4), arm64 and x86_64 (qemu for ARM).
- libass `checkasm` on armv7 with both engines, seeds 1/22/333: all 38 pass.
- Byte compare of every frame of the 4 libass-nv samples + the stress tracks:
  ours, nova (performance mode off) and hybrid are **identical to stock** on
  each arch. Performance mode (`EDGES=1`) changes every frame.
- The app's libass tests (`tests/ass_*.sh`, `asssinais`, `legenda_idle_ass`,
  `mkvass_alem_teto`) give the same results with stock and hybrid.
- `assrender.c`, `assperf.c`, `mkvass.c`, `video_tpk.c` compile against the
  hybrid header with `-DNV_TPK` and `-DNV_TPK -DNV_TPK40` and strict flags.

## Speed so far (x86_64 here, 4 cores; ms for the whole track, single thread)

| Track | Stock | mpvNova | Hybrid |
|---|---|---|---|
| clipwipe (blurred drawing, moving clip) | 184 | 574 | 198 |
| signs (4 moving blurred signs per frame) | 620 | 1064 | 587 |
| heavy (signs + clip + karaoke + dialogue) | 1045 | 1406 | 997 |
| fx (karaoke template, ~200 events on screen) | 144 | 100 | 106 |

## Speed on a real ARM core (GitHub arm64 runner, Neoverse N2, 4 cores; armv7 code ran natively)

`.github/workflows/libass-bench.yml`, run 45529408672. Whole-track ms, best of 3,
armv7 build (what every .tpk runs). The TV's cores are slower; the ratios are
what to look at.

| Track | Stock | Ours today (3 blur threads) | mpvNova (default threads) | Hybrid, our engine | Hybrid, mpvNova engine |
|---|---|---|---|---|---|
| clipwipe | 355 | 405 | 693 | 380 | 333 |
| signs | 1364 | 942 | 1207 | 986 | 689 |
| heavy | 2645 | 1861 | 1991 | 1321 | 1327 |
| fx (many events) | 253 | 224 | 71 | 87 | 72 |
| all 8 tracks | 4930 | 3700 | 4138 | 2971 | **2598** |

Hybrid with mpvNova's engine: 30 % less libass time than today's .tpk and
47 % less than stock, with the same bytes. Performance mode (`EDGES=1`)
would only save another 4 % and changes every frame, so it stays off.
