# Production Ficha row: approved V1

V1 replaces the previous poster plus ghosted metadata island. Existing `FIL_TIPO_FICHA` preferences keep their identity; there is no new preference or internal variant switch. V2/V3 drawing and exploration-only build checks were removed.

At TV 1080p, inactive items are intact 2:3 posters. Focus opens a 470 px extension from the poster's left anchor and lifts the whole card 10 px. The extension uses opaque `#121316`, with a short native dithered shadow and 22 px corners. DESIGN.md prohibits decorative outlines. The hero keeps its logo; title, year, genres, IMDb and up to three overview lines belong to the selected card. No row counter or control-hint bar is added.

Existing focus springs drive incoming and outgoing widths without resets. Neighbours use their combined current width; scrolling reserves the complete incoming card. Pointer targets, context-menu anchor and context-menu repaint share the production geometry. Touch drag retains scroll ownership. Phone portrait increases poster and text size and caps the extension to available width.

Metadata renders into one reusable `GfxMini` texture. Its key includes catalog revision, profile session, item, font, language, overview size, rating, dimensions and pixel scale. Animation changes only positions, clip and alpha. Poster requests retain a fixed width; no animated-width texture promotion, background photo, blur or network work is added to the card draw. Missing artwork uses a solid surface and title fallback. Reduced-motion policy remains the existing Home policy.

Run from the worktree root:

```sh
bash tests/fileirarepensar_shot.sh
bash tests/home.sh
bash tests/row_navigation.sh
bash tools/build-local.sh
```

Evidence defaults to `/Volumes/ExternalSSD/tmp/rev/fileirav1/`:

- `tv-first.png`, `tv-middle.png`, `tv-unfocused.png`, `tv-last.png`: production TV 1920 × 1080.
- `animation-strip.png`: four real frames of focus transfer, including the fully settled endpoint. Remaining updates always run; animation is never shortened for capture.
- `v1-side-by-side.png`: approved `/Volumes/ExternalSSD/tmp/rev/fileirarepensar/v1.png` left, production first-item capture right.
- `phone-first.png`, `phone-touch.png`, `phone-fallback.png`: portrait production rendering, pointer selection and missing-art/long-title fallback.

The harness compiles production sources with touch support, injects the frozen catalog and drives actual key events, pointer selection, updates, texture loading and OpenGL rendering. Checks cover focus springs and reversals, row-end bounds, hero return, context repaint, cache reuse, touch scroll ownership and late catalog changes. Home layout tests cover expansion geometry; existing row navigation runs both normal and Ficha styles.

Limits: desktop native OpenGL captures and regression checks. Weak-TV frame time and physical remote/touch behavior require device verification. No deployment or push is performed.
