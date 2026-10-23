# Vidro fosco: backdrop-only source (2.0.5)

Owner, 16/10: "o frost ta uma merda ainda, ou a gente pesquisa outra abordagem
ou vamos tirar".

## Why the old path costs

`gfx_vidro` -> `vidroFonte` -> `vidCapturar` (src/gfx.c). Each capture:

1. `glCopyTexSubImage2D` of the **whole screen** (1920x1080, 4K on some TVs)
   into `vidGrab`, mid-frame. On a tiler (Mali-G52/G71 in the TCL/C9, the
   webOS/Tizen SoCs) that forces the tile list drawn so far to resolve to
   memory before the copy, then the frame continues with a reload.
2. Five full-screen passes: 1/2 box, 1/4, two Kawase, back to 1/2.

Reuse across frames works only while `vidroHash` (every draw that reaches what
the glasses read) is unchanged. On Home the top bar, avatar and clock island
read the band where the rows scroll and the focus ring moves, so every
scrolling/focus frame recaptured: roughly 2.1 M px copied + ~1.3 M px of
blur fill per frame at 1080p (4x at 4K), plus the tile flush. The old
`perf(glass)` commits (d98d72026, 0c77460c1, 5536ee8da, d7ec89975) cut the
number of captures to one per frame, not that one.

It is also ugly: the glass blurs whatever passes under it (poster edges,
text, focus ring), so the bar shimmers while rows scroll. The green cast
(14/10) came from the old palette source, fixed by reading the screen
(tests/vidro_neutro.sh); nothing below reintroduces palette colour.

## Alternatives (cost per frame, Mali-class GPU, 1080p)

| | Moving content (scroll/focus) | Backdrop changes (hero swap, detail scroll) | Static | Look |
|-|-|-|-|-|
| old: capture all under glass | 1 copy + 5 passes | same | 0 | blurs posters/text, shimmers |
| (a) pre-blur the art once (`gfx_desfocado`, 96x54) | 0 | 0 (one 96x54 blur per new art) | 0 | needs per-screen art + rect mapping; loses the hero ramps, light and veils, so glass over the dark ramp shows bright art |
| (a') **backdrop-layer capture** (chosen) | 0 | 1 copy + 5 passes | 0 | exact composite of art + ramp + light + veils; no posters/text |
| (b) 1/8-res capture + dual Kawase on change | 1 small copy + 4 tiny passes | same | 0 | still blurs content; 1/8 shows blocky shimmer |
| (c) tinted translucent panel, no blur | 0 | 0 | 0 | gray, not frosted |

(a') is (a) done generically: the "pre-blurred backdrop" is the screen's own
backdrop layer, captured the frame it changes, so every screen (Home hero,
detail art, Settings Frost/Arte borrada, Explore sky, social background)
works with no per-screen code and no colour or ramp mismatch.

## What runs now

- The backdrop layer = draws before the frame's first content draw: hero,
  detail, vitrine, fundo, snap/ambient light, sky, veils, edge light, and any
  full-screen draw (`vidroFundoModo`).
- The first content draw (or the first glass) closes the layer; if its hash
  changed it is captured into `vidFundo` (half res, same 5-pass kernel, same
  shader: no colour change). Only when the last frame had glass.
- Every backdrop draw enters its hash wherever it lands, so the copy is valid
  for a glass anywhere (the Explore focus glass moving).
- Glasses sample `vidFundo`. Fallbacks to the old capture: first glass frame,
  a full-screen draw after the layer (modal veil).
- (c) stays the fallback it already was: video under the glass
  (`gfx_vidro_fosco_bloquear`), "Vidro fosco" off, minimal effects -> flat
  tinted scrim.

## A/B switch

`NV_VIDRO_SO_FUNDO` in src/gfx.c (default 1). Build with
`-DNV_VIDRO_SO_FUNDO=0` (or set `gfx_vidro_so_fundo = 0`) for the old path.
"Vidro fosco" in Settings still turns all blur off.

## Measured (Mac, offline harnesses)

- tests/vidro_orcamento.sh, rows scrolling over a static backdrop:
  old 1 capture + 5 passes/frame, 0.38 ms/frame; new 0 captures, 0.18 ms/frame.
- tests/vidro_c9.sh, Home with menu focus moving and the backdrop changing
  every frame (all tiers, 1080p/4K): 1.15 -> 0.35 captures/frame, 5.75 ->
  1.75 passes/frame, max 2 -> 1; webOS 1080p blur pixels/frame 1.64 M ->
  0.50 M (4K 6.56 M -> 2.00 M).
- tests/vidro_telas.sh, moving: detail comments 0.20 -> 0, Explore 0.54 ->
  0.04 captures/frame; detail sources, Search, Settings already 0, still 0.
- Before/after PNGs (home, detail, settings, explore, search) in
  /Volumes/ExternalSSD/tmp/fosco-nova/{antes,depois}; stacked in cmp/.
  Home and Settings unchanged; under the glass only the backdrop now.
