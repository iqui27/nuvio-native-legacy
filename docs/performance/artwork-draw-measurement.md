# Artwork draw measurement (opt-in)

Diagnostics only. No render, cache, or pipeline change. No TV numbers in this
doc: nobody has run it on a TV yet.

## Gap it fills

`[tex-trace]` (src/tex_cache.c, gated by `NV_TEX_TRACE_MS` 250 ms / logo
requests) already logs network, fetch, decode queue, decode sizes, upload queue
and upload CPU time. `upload-total` (src/tex_cache.c:4624) logs `bytes` only.
Nothing logged the decoded **w x h** next to the **physical px** a draw
actually covers. `[art-draw]` pairs the two.

## Enable

`NV_ART_DRAW_MEASURE=1` in the app environment. Anything else, or unset, means
off. Read once on the first textured `gfx_rect`.

Android: a normal launcher start does not inherit `adb shell` env, so the
probe is also on when the system property `debug.nuvio.artdraw` is exactly
`1` (same pattern as `debug.nuvio.perf` / `debug.nuvio.gputempo`). Unset,
empty, `0` or any other value means off. Read at the same single point as the
env, never per frame. Not validated on a physical TV yet.

- Off: `gfx_rect` does `if (arte_medida_ligada && tex)`. One int compare, no
  lock, no scan, no log.
- On: dedup runs first (no lock, at most 48 slots). The cache lookup
  (`tex_dims_gl`, which takes the mutex and scans `itens`) runs only on a dedup
  miss. A line is printed only for a new resource, or when `dst` w or h
  changes by 25% or more.
- Failed lookups (not a ready cache texture, or a bad size): the name is
  remembered (up to 48 names, no lock) and never looked up again.
- Lookup budget: `ARTE_MEDIDA_BUSCAS` = 192 lookups per process, hits and
  misses together. When it runs out the probe sets itself to off. So a stream
  of unrecognized textures costs at most 192 locked scans, then nothing.
- Hard cap: `ARTE_MEDIDA_TETO` = 48 lines per process. After that the probe
  sets itself to off, and the disabled path above applies again.

## Line

```
[art-draw] art=1 n=1 mode=1 dst=960x540 vis=960x540 rt=1920x1080 tex=300x450 fit=1 src=300x169 px_per_texel_milli=3200x3200 layer_milli=1500 px_per_unit_milli=2000
```

| field | meaning |
|---|---|
| `art` | Process-local ordinal: the order in which this run first saw the texture. Not persistent. Not derived from the URL, hash or GL name. |
| `n` | Line count in this process (at most 48). |
| `mode` | `GfxModo` integer (src/gfx.h). |
| `dst` | Whole quad in render-target px, after the group transform (`GFX_TR_RECT`) and the layer scale (`escAtiva`). This is what was **drawn**, not what the layout requested. |
| `vis` | `dst` clipped to the render target and the active scissor (`recorteBox`). |
| `rt` | Render target px (`telaW x telaH`). |
| `tex` | Decoded texture size from the cache (`itens[].w/h`, set at upload, src/tex_cache.c:4612). |
| `fit` | Integer: how the shader maps the image. `0` stretch = `gfx_tex_aspect_atual` is 0. `1` cover = `coverAsp` (src/gfx.c:479), excess axis cropped. `2` contain = the `GFX_CARD` rule (ratio off by more than 25%, no `gfx_card_forcar_cover_atual`). The ratio is `uAspect / uTexAsp` with `uAspect` = layout `r.w / r.h`, exactly what the shader gets, not the px aspect. |
| `src` | Texels inside the quad (cover crops; contain/stretch keep everything). |
| `px_per_texel_milli` | x/y physical px per texel x1000 for the whole mapped image. x uses `telaW / NV_TELA_W`, y uses `telaH / NV_TELA_H`; they differ when the target scale is not uniform. Above 1000 the texture is upscaled on screen; below 1000 it is downscaled. |
| `layer_milli` | `escAtiva * gfxTr.s` x1000. |
| `px_per_unit_milli` | Render-target px per layout unit x1000 (`telaW / NV_TELA_W`). |

## Reused state (no new timers, no GL queries)

- Decoded size: `itens[].w/h` through `tex_dims_gl` (src/tex_cache.c:4386), the
  same fields `tex_aspecto` reads (src/tex_cache.c:4372).
- Aspect: `gfx_tex_aspect_atual`, the value sent as `uTexAsp`.
- Scratch scope: `arte_medida_fora` (save/restore, `ARTE_FORA_INI/FIM`) set
  by `gfx_borrao_gerar` and `gfx_desfocado`, which bind their own FBO without
  changing `telaW/H`; checked before dims, ordinal and cap.
- Scale/target: `escAtiva`, `gfxTr`, `telaW/H`, `recorteBox`. Same conversion
  as `fillGpuArea` (src/gfx.c:124).
- Hook: one line in `gfx_rect` right before the program bind, after every
  early return (src/gfx.c:2742), helper `arteMedir` (src/gfx.c:2496). Pure
  math, strip exclusion, dedup, miss memory, budget and cap live in
  src/arte_medida.c.

## Coverage and limits

- Covered: every textured `gfx_rect` on the screen target that reaches the
  draw and whose texture is a READY tex_cache texture, drawn whole (`uSub` =
  0,0,1,1). That includes `gfx_arte_veu` and texA of `gfx_hero_camadas`.
  The screen target may itself be an FBO (gpunivel internal target): it is
  measured, the policy never looks at the FBO name.
- Not covered: `gfx_rect_furos` (no caller in src/ today; its strips carry a
  computed `uSub`, so even a whole-rect strip is not reliably 0,0,1,1) and
  any other partial `uSub` window
  (skipped before the lookup; no strip/hole crop telemetry);
  texB of `gfx_hero_camadas` (bound directly); text (`GFX_TEXTO`);
  the Texture-mode title surface (`GFX_TEXTURA`); FBO/snapshot/mini/ambient
  passes (`telaW/H` not the screen); `gfx_borrao_gerar`/`gfx_desfocado`
  scratch passes (scratch scope, no lookup, no ordinal); the gpunivel
  upscale/RCAS copies sample `intTex`/`rcasTex`, not cache textures (one
  remembered miss each, no line); rotated groups; and any texture
  that is not in the cache (lookup fails, no line).
- The probe runs after every early return of `gfx_rect` (light/minimal
  effects, disabled modes, ambient veil, full-screen clear), so a line means
  the quad was submitted.
- Approximations: `dst/vis` are the rect extent; they ignore rounded corners,
  alpha and per-mode shader tweaks (hero parallax, mode-specific crops such as
  the `dispW` path). `fit/src/px_per_texel_milli` model `coverAsp` and the
  card contain rule only, for every mode, even modes whose shader samples
  differently.
- `rt` is the render target. When gpunivel renders to an internal target
  (`[gpu-nivel] ampliar=... WxH -> WxH`, src/gpunivel.c:563), the panel px
  are `rt` scaled by that ratio afterwards. This probe does not include that
  final scale.
- Dedup and miss memory key on the GL name. GL can reuse a name for another
  image: same name at the same size is not logged again, and a name that once
  missed is never logged. `art` is not a unique image identity.
- **No request timeline.** `[art-draw]` lines carry no hash, so they cannot be
  joined to `[tex-trace]` lines (FNV hash of the path). The tex-trace hash also
  identifies a resource, not a request generation. Matching a draw to its
  fetch/decode/upload timing is still unsolved. Use the two logs only for
  separate, side-by-side reading.
- Old logs: the one log verified as app 2.0.8 has an unknown exact commit;
  every other old log has an unknown version. Both are historical only, never
  a current baseline, and have no `[art-draw]` lines.

## Passive procedure

1. Start the build with `NV_ART_DRAW_MEASURE=1`, the same way the owner
   normally runs it. Do not script keys, relaunch, clear data or inject
   anything.
2. The owner navigates as usual. The probe stops after 48 lines.
3. Collect stdout. Keep `[art-draw]`, `[gpu-nivel]` and `[tex-trace]` lines.
   Read them separately (see "No request timeline").

Android TV (needs separate authorization; nothing here was run):

1. Save the old value: `adb shell getprop debug.nuvio.artdraw`.
2. Before the owner's next normal start: `adb shell setprop debug.nuvio.artdraw 1`.
   Do not launch the app, send keys or clear logcat.
3. Listen passively (`adb logcat`) while the owner uses the TV as usual.
4. Afterwards restore the saved value (`adb shell setprop debug.nuvio.artdraw
   <old>`, or `""` if it was unset).

## Check

`bash tests/arte_medida.sh` compiles src/arte_medida.c with tests/arte_medida.c
under a fresh temp `HOME`, and fails closed if that `HOME` is not isolated. It
also builds src/arte_medida.c with `-DNV_ANDROID` against
tests/stub_android (a stand-in `<sys/system_properties.h>`) and a fake
`__system_property_get`: property unset/empty/`0`/`11`/`true` off, exactly
`1` on, one read per resolve, env switch still on. It
checks: default off; `gfx_rect_furos` strip skipped with no lookup and no
ordinal; scratch scope (nested, restored) skipped with no lookup and no
ordinal; a 1280x720 FBO screen target measured; cover crop at a 1.5 layer scale and 2 px/unit; card contain;
stretch; integer `fit`; non-uniform target (x 2, y 1) where the px aspect
would wrongly pick contain/crop but `uAspect` does not; target and scissor
clipping; invalid input (zero, NaN, a texture not in the cache, an empty
quad); dedup without a lookup; 25% change; repeated misses with zero repeat
lookups; 99000 unique unknown names within the 192-lookup budget, then off;
and the 48-line cap that turns the probe off. The `gfx_rect` hook and
`tex_dims_gl` are covered by the build only. The test runs no GL.
