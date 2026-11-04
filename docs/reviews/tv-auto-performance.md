# Automatic TV performance policy

Owner rule: changes must feel invisible when possible — speed first, visuals only as a last resort.

## What is measured

`gpun_janela_elegivel` (`src/gpunivel.c`) is the one definition of a clean window, shared by the effects ladder and the Automatic 4K probe. A window qualifies only with full **Home** or **browse** frames (Explore, Library, Search) with decoded artwork on screen, no video, player or trailer, and no frame inside the **3 s** settle after a scene/layout/live-color change or texture upload. Frames with CPU submission **> 8 ms** or duration **> 100 ms** are dropped from the FPS; if they exceed 20% of the window, fewer than 10 clean frames remain, or a stall exceeds **1 s**, the window is invalid. Glass setting changes are not a settle trigger: glass is not part of the scene key in `main.c`.

Reports use the existing **~3 s** FPS window in `main.c`. The ladder takes the refresh from `SDL_DisplayMode.refresh_rate` at startup, capped at **60 Hz**, with a **16.70 ms** fallback when it is unknown.

Host tests cannot show what a given TV's SDL reports: a 50 Hz panel reported as 0 or 60 gets the 60 Hz thresholds.

## Effects ladder (`gpun_degrau`, automatic settings only)

Manual **Full / Light / Minimal** and forced glass/frost/animation choices skip the ladder but keep measuring where applicable. A forced setting still consumes its rung, without any effect.

| Step | Automatic reduction | Notes |
|-----:|---------------------|-------|
| 1 | **One-pass blur** | `ajustes_vidro_qualidade() == 3`; one Kawase pass instead of two, glass stays frosted. Replaces the old rung that went straight to smoked glass, which left rung 2 almost nothing to save |
| 2 | Frost **off** | First rung that removes blur. Persisted `foscoVetado` after proven gain |
| 3 | gfx **light budget** | `gfx_definir_efeitos_leves`: cheaper dither, no decorative highlights. Only with Animations on Automatic |
| 4 | Glass **off** | Last resort |

**Motion:** no rung changes motion. `ajustes_animacoes_reduzidas()` follows only the explicit Reduced choice (`tests/tela21.c`); rung 3 is keyed to the Animations setting but only sets the render budget.

**GPU effect levels** (0/1/2) map from active ladder bits: level **2** when step 4 is active, **1** when step 3+, else **0**.

### Trials

- One at a time, across effects and resolution: the ladder holds while the Automatic 4K probe is pending, and the 4K verdict and manual 4K watch skip windows of a pending effects trial.
- A scene change or invalid window **reverts** a pending trial.
- A step is kept only after **three** clean windows with **≥10%** and **≥3 FPS** gain vs the trial baseline.
- Failed trials are skipped for the rest of the boot (`semGanho`).

### Slow / fast streaks

- **Three** consecutive reports below `refresh − 5` FPS advance **one** rung (~9 s); never skip rungs.
- **Ten** consecutive reports at or above `refresh − 2` FPS start **recovery** one rung down (~30 s). Recovery is applied directly; a slowdown afterwards re-enters through a measured trial.
- Stalls, invalid windows and ineligible frames reset streaks.
- Failed recovery waits **100** eligible reports (~5 min) before retry; no permanent floor. Reaching step 0 with ten more fast reports clears the failed-recovery mark.

## Resolution (`src/resolucao.h`)

| Setting | Behavior |
|---------|----------|
| **Automatic**, 1080p panel or 4K refused | 1080p; nothing to probe |
| **Automatic**, 4K panel, no verdict | Launches on the **4K surface** to probe it |
| **4K** | Watched; falls back and saves `resolucao-4k-recuo.txt` until 4K is picked again |
| **1080p / 720p** | Manual, never changed automatically. **720p** is never chosen automatically |

Automatic probe: two clean windows with GPU p90 below the pass limit, or FPS at or above it without a GPU clock. One window beyond the bad limit fails at once. The verdict is saved per app version in `resolucao-auto.txt`. An approved 4K demotes after 3 bad reports in a row. A probe with **20** visible-interface reports and no clean window drops to 1080p for the session and saves nothing, so a TV whose frames always look CPU-bound does not stay on the expensive surface.

Limits follow the panel: 50–59 Hz scale; unknown, faster and sub-48 Hz modes use the 60 Hz row, so 4K30 stays rejected in favor of the 1080p the same TV runs faster.

| Limit | 60 Hz | 50 Hz |
|-------|-------|-------|
| Auto pass: GPU p90 under / FPS at least | 12 ms / 55 | 14.4 ms / 45 |
| Auto bad: GPU p90 over / FPS under | 25 ms / 40 | 30 ms / 33.3 |
| Manual watch bad, 4 reports: GPU median over / FPS under | 30 ms / 40 | 36 ms / 33.3 |

The manual watch counts GPU-clock windows on any interface screen; bare FPS needs a clean window.

Falling back mid-session (`gpun_alvo_1080`) draws into a 1920x1080 target, re-keys `gpu-nivel.txt` and measures effects again from full quality.

## App upscaling (`gpun_ampliar_app`)

Runs only with an explicit **1080p/720p** resolution, a 4K panel, the person's opt-in and GPU support (`gpun_upscale_suportado`: no Mali-4xx, fragment highp). On Automatic and 4K the TV scales, except the session of an automatic fallback, which scales its internal target with bilinear. Algorithm **Automatic** picks fractional **SGSR** or integer **sharp bilinear**; **EASU/RCAS/Lanczos** run only when picked. The Settings rows go inactive from `gpun_ampliacao_ativa`, the session's real state. The ladder never changes upscaling: it is an explicit choice, and there is no TV measurement of its cost.

## Persistence

`gpu-nivel.txt` version **1**: GPU/driver/firmware/surface **fingerprint**, `foscoVetado`, `vetoArranques`, legacy FPS fields. Rungs are not saved: every launch starts from full recoverable quality. The frost veto is bounded: on the same fingerprint it expires after **20** launches and frost must prove itself again. Explicit Frost Automatic clears it on disk, including under manual effects.

## Settings truth

- `gpun_efeitos_automaticos()` is true for any active rung, or the frost mark while Frost is Automatic; never under manual effects. It drives the Settings help warning; levels 0 and 1 share the "light" wording.
- `gpun_fosco_vetado()` is the ladder's frost state under automatic effects, and false under manual effects.
- The Settings row and help for Automatic resolution do not show the effective resolution; only the `[4k]` log does. The help line "começa em 1080p" does not describe the probe on a 4K panel.

## Explicit non-goals

- No `glFinish` / GPU blocking for measurement.
- No layer-specific native text shortcuts without evidence.
- `gfx_vidro_governar` has no production caller; only `tests/vidro_qualidade.c` runs it.
- Host/unit tests prove policy logic only — **not** physical TV proof.

## Regression tests

From repo root (set `TMPDIR` to a local SSD path):

- `bash tests/gpunivel_transients.sh` — refresh limits, shared eligibility, serialized trials, bounded persistence, transients, manual overrides
- `bash tests/gpuauto.sh` — reporter and resolution wiring in `main.c`, sustained FPS ladder
- `bash tests/gpunivel.sh` — adaptive rules + resolution/effects independence
- `bash tests/gpunivel_ajuda.sh` — settings help for every automatic reduction
- `bash tests/vidro_qualidade.sh` — one-pass blur on real GL
- `bash tests/resolucao.sh` — 60 Hz thresholds, verdict memory, migration

Implementation lives in `src/gpunivel.c`, `src/gpunivel.h`, `src/main.c`, `src/ajustes.c`, `src/gfx.c`, `src/resolucao.h`.
