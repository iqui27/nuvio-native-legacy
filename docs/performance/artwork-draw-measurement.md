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

- Off: `gfx_rect` does `if (arte_medida_ligada && tex)`. One int compare, no
  lock, no scan, no log.
- On: dedup runs first (no lock, at most 48 slots). The cache lookup
  (`tex_dims_gl`, which takes the mutex and scans `itens`) runs only on a miss.
  A line is printed only for a new resource, or when `dst` w or h changes by
  25% or more.
- Hard cap: `ARTE_MEDIDA_TETO` = 48 lines per process. After that the probe
  sets itself to off, and the disabled path above applies again.

## Line

```
[art-draw] art=1 n=1 mode=1 dst=960x540 vis=960x540 rt=1920x1080 tex=300x450 fit=cover src=300x169 px_per_texel_milli=3200x3200 layer_milli=1500 px_per_unit_milli=2000
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
| `fit` | How the shader maps the image. `cover` = `coverAsp` (src/gfx.c:479), excess axis cropped. `contain` = the `GFX_CARD` rule (aspect off by more than 25%, no `gfx_card_forcar_cover_atual`). `stretch` = `gfx_tex_aspect_atual` is 0. |
| `src` | Texels inside the quad (cover crops; contain/stretch keep everything). |
| `px_per_texel_milli` | x/y physical px per texel x1000 for the whole mapped image. Above 1000 the texture is upscaled on screen; below 1000 it is downscaled. |
| `layer_milli` | `escAtiva * gfxTr.s` x1000. |
| `px_per_unit_milli` | Render-target px per layout unit x1000 (`telaW / NV_TELA_W`). |

## Reused state (no new timers, no GL queries)

- Decoded size: `itens[].w/h` through `tex_dims_gl` (src/tex_cache.c:4385), the
  same fields `tex_aspecto` reads (src/tex_cache.c:4372).
- Aspect: `gfx_tex_aspect_atual`, the value sent as `uTexAsp`.
- Scale/target: `escAtiva`, `gfxTr`, `telaW/H`, `recorteBox`. Same conversion
  as `fillGpuArea` (src/gfx.c:124).
- Hook: one line in `gfx_rect` after the Texture-mode swap
  (src/gfx.c:2570), helper `arteMedir` (src/gfx.c:2493). Pure math, dedup and
  cap live in src/arte_medida.c.

## Coverage and limits

- Covered: every textured `gfx_rect` on the screen target whose texture is a
  READY tex_cache texture. That includes calls made through `gfx_rect_furos`,
  `gfx_arte_veu`, and texA of `gfx_hero_camadas`.
- Not covered: texB of `gfx_hero_camadas` (bound directly); text (`GFX_TEXTO`);
  the Texture-mode title surface (`GFX_TEXTURA`); FBO/snapshot/mini/ambient
  passes (any target other than the screen); rotated groups; and any texture
  that is not in the cache (lookup fails, no line).
- The probe runs before the later early returns in `gfx_rect` (light/minimal
  effects, disabled modes). A line for one of those modes can exist even
  though that draw was skipped. Art modes do not take those returns.
- `vis` ignores `gfx_rect_furos` holes and per-mode shader tweaks (hero
  parallax, mode-specific crops such as the `dispW` path). `fit/src` model
  `coverAsp` and the card contain rule only.
- `rt` is the render target. When gpunivel renders to an internal target
  (`[gpu-nivel] ampliar=... WxH -> WxH`, src/gpunivel.c:563), the panel px
  are `rt` scaled by that ratio afterwards. This probe does not include that
  final scale.
- Dedup keys on the GL name plus `dst`. A GL name reused for another image at
  the same size is not logged again.
- **No request timeline.** `[art-draw]` lines carry no hash, so they cannot be
  joined to `[tex-trace]` lines (FNV hash of the path). The tex-trace hash also
  identifies a resource, not a request generation. Matching a draw to its
  fetch/decode/upload timing is still unsolved. Use the two logs only for
  separate, side-by-side reading.
- Logs captured before this commit have no `[art-draw]` lines. Treat them as
  historical, with an unknown version.

## Passive procedure

1. Start the build with `NV_ART_DRAW_MEASURE=1`, the same way the owner
   normally runs it. Do not script keys, relaunch, clear data or inject
   anything.
2. The owner navigates as usual. The probe stops after 48 lines.
3. Collect stdout. Keep `[art-draw]`, `[gpu-nivel]` and `[tex-trace]` lines.
   Read them separately (see "No request timeline").

## Check

`bash tests/arte_medida.sh` compiles src/arte_medida.c with tests/arte_medida.c
under a fresh temp `HOME`, and fails closed if that `HOME` is not isolated. It
checks: default off; cover crop at a 1.5 layer scale and 2 px/unit; card
contain; stretch; target and scissor clipping; invalid input (zero, NaN, a
texture not in the cache, an empty quad); dedup without a lookup; 25% change;
and the 48-line cap that turns the probe off. The `gfx_rect` hook and
`tex_dims_gl` are covered by the build only. The test runs no GL.
