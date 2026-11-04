# Automatic TV performance policy

Owner rule: changes must feel invisible when possible — speed first, visuals only as a last resort.

## What is measured

Only full **Home** and **browse** frames (Explore, Library, Search) with decoded artwork on screen. Player, trailers, opening animations, empty Home, texture upload bursts, and the **3 s settled window** after scene/layout/glass/live-color changes do **not** count.

Reports use the existing **~3 s** FPS window in `main.c`. Thresholds use the panel refresh when available (`SDL_DisplayMode.refresh_rate`), capped at **60 Hz**, with a **16.70 ms** fallback when refresh is unknown or invalid. **50 Hz** panels use **45/48 FPS** cutoffs (not 55/58).

Per-frame samples drop CPU-bound windows (submission > **8 ms** average or isolated **>100 ms** frames) before a report is emitted.

## Effects ladder (`gpun_degrau`, automatic settings only)

Manual **Full / Light / Minimal** and forced glass/frost/animation choices skip the ladder but keep measuring where applicable.

| Step | Automatic reduction | Notes |
|-----:|---------------------|-------|
| 1 | Glass **Performance** quality | Trial before staying |
| 2 | Frost **off** | Persisted `foscoVetado` in `gpu-nivel.txt` after proven gain; explicit Frost Automatic clears the mark |
| 3 | Reduced **animations** | Uses `ajustes_efeitos_automaticos()` light budget |
| 4 | Glass **off** | Last resort |

**GPU effect levels** (0/1/2) map from active ladder bits: level **2** when step 4 is active, **1** when step 3+, else **0**.

### Trials (quality steps)

- One trial at a time; scene change **reverts** a pending trial.
- A step is kept only after **three** clean windows with **≥10%** and **≥3 FPS** gain vs the trial baseline.
- Failed trials are skipped for the rest of the boot (`semGanho`); frost veto is hardware-scoped in `gpu-nivel.txt`.

### Slow / fast streaks (forced steps while trialing)

- **Three** consecutive reports below `refresh − 5` FPS advance **one** rung (~9 s), even when that rung is forced off — never skip rungs.
- **Ten** consecutive reports at or above `refresh − 2` FPS start **recovery** one rung down (~30 s), with per-rung trials.
- Stalls **>1 s**, invalid/mixed windows, and ineligible frames reset streaks and cancel pending trials.
- Failed recovery waits **100** eligible reports (~5 min) before retry; no permanent floor.

## Resolution (independent)

See `resolucao.h`: **Automatic** stays at **1080p** pixels; slow GPUs lose **effects**, not resolution. **720p** is manual only. **4K** probe/fallback and **1080 internal target** on 4K (`gpun_alvo_1080`) are separate; changing target re-keys `gpu-nivel.txt` and re-probes effects from full quality.

## App upscaling (`gpun_ampliar_app`)

Does not enable blindly: requires panel eligibility from settings, GPU support (`gpun_upscale_suportado` — no Mali-4xx / no highp), and person opt-in. Algorithm **Automatic** picks fractional **SGSR** or integer **sharp bilinear**; **RCAS/FSR/EASU** only on supported paths — no claimed TV speedup without measurement.

## Persistence

`gpu-nivel.txt` version **1**: GPU/driver/firmware/surface **fingerprint**, optional **`foscoVetado`**, legacy FPS fields; session restores recoverable quality on launch. Rejected effect trials are **not** a permanent block across boots except frost veto on the same fingerprint.

## Settings truth

- `gpun_efeitos_automaticos()` / `gpun_nivel()` drive help text and effective glass quality (`ajustes_vidro_qualidade`).
- `gpun_fosco_vetado()` reflects automatic frost off or ladder step 2+.

## Explicit non-goals

- No `glFinish` / GPU blocking for measurement.
- No layer-specific native text shortcuts without evidence.
- Host/unit tests prove policy logic only — **not** physical TV proof.

## Regression tests

From repo root (set `TMPDIR` to a local SSD path):

- `bash tests/gpunivel_transients.sh` — transients, trials, recovery, manual overrides, 50/60 Hz, fingerprint
- `bash tests/gpuauto.sh` — reporter wiring + sustained FPS ladder
- `bash tests/gpunivel.sh` — legacy adaptive rules + resolution/effects independence
- `bash tests/gpunivel_ajuda.sh` — settings help when automatic reductions are active

Co-authored policy text; implementation lives in `src/gpunivel.c`, `src/gpunivel.h`, `src/main.c`, `src/ajustes.c`, `src/resolucao.h`.
