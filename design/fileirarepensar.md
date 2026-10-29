# Top Rated row: three native design studies

Status: exploration only. No selected direction, production setting, merge, push, or deployment.

The existing style is `FIL_TIPO_FICHA` (`src/fileiras.h`), mapped by `tipoDaEscolha` in `src/home.c` to `FILEIRA_FICHA`. `larguraDe()` reserves `NV_FICHA_W` for every item; `desenhaFicha()` paints the separate metadata island with `0.06 + 0.94 * focus` opacity. The inactive islands cause the ghost text. The far-right position pill comes from the common row heading in `home_desenhar_()`.

## V1 — Expand into the artwork

The intact portrait anchors a single expanded card. Its right side opens into a dimmed backdrop with title, year, genres, IMDb rating, and three synopsis lines. Inactive items are plain posters. The hero retains only a logo, leaving the reading task with the selected card. No counter or decorative outline.

Animation: the existing focus spring expands width from the portrait to approximately 685 px and lifts the card 10 px. Neighbours move by the same width delta; the outgoing card closes simultaneously. Incoming copy reveals within a fixed text measure rather than rewrapping on every frame. Outgoing copy disappears when focus leaves, so there are never two metadata blocks. The native spring reaches 95% in approximately 120 ms; the harness continues for 120 update frames after the key event to capture the completed state.

Tradeoff: preserves portrait recognition and local reading, but horizontal movement is greater than in V2. Recommended when metadata must stay beside selection.

## V2 — Posters first, details in the hero

A dense, clean portrait row; the focused poster grows 8%, rises slightly, and gains a short shadow. All metadata lives in the existing hero, whose native artwork and copy transition remains intact. Eight posters are visible at the reference width. No duplicate synopsis, inactive metadata, counter, or focus outline.

Animation: the incoming poster rises and grows on the native focus spring as the outgoing poster settles. Poster positions remain stable. The existing hero transition completes independently, including its current focus-settle delay; the midpoint intentionally shows the previous hero while the poster focus is moving. Full settlement is captured, not cut short.

Tradeoff: the eye travels between row and hero. Recommended overall: best browsing density and least layout movement.

## V3 — Landscape screening room

Landscape neighbours surround a selected 640 × 322 px backdrop with its logo over a dithered lower veil. Year, genres, IMDb rating, and a two-line synopsis sit directly beneath that card. The row reserves its caption height, keeping the next heading below it. The hero keeps a restrained logo; all detailed reading stays attached to the selected card.

Animation: the selected landscape grows from 390 × 219 px to 640 × 322 px and rises 10 px. Adjacent cards move with the same spring-driven width change. Only the incoming card owns the metadata band; its opacity follows focus, with no inactive text. Full spring and hero settlement are included in the last frame.

Tradeoff: strongest focus hierarchy, but fewer titles fit and the metadata band increases row height. Recommended for a deliberate, cinematic browsing pace.

## Evidence and reproduction

Artifacts: `/Volumes/ExternalSSD/tmp/rev/fileirarepensar/`.

- `current-reference.png`: untouched owner screenshot, including quick-settings overlay.
- `current.png`: current native Ficha style with the same unobstructed fixture and TV scale as the alternatives.
- `v1.png`, `v2.png`, `v3.png`: settled 1920 × 1080 captures, Sopranos selected in Top Rated.
- `v1-animation.png`, `v2-animation.png`, `v3-animation.png`: 5760 × 1080 strips. Left: Wonderfools selected; middle: native focus transfer to Sopranos after two update frames (focus approximately 0.565); right: Sopranos fully settled. These are samples from a complete transition, not a claim of TV frame-time measurement.
- Individual `vN-frame0.png` and `vN-frame1.png` are retained at full 1080p.

Run from the worktree root:

```sh
bash tests/fileirarepensar_shot.sh
bash tests/fileirarepensar_build.sh
```

The first command compiles the real app sources with a shot entry point, injects a frozen `CatItem` catalog, and drives actual key events, `home_atualizar()`, texture loading, focus springs, hero rendering, text, and shader paths. `NV_FICHA_EXPLORATION` exists only in that entry point; `fichaExploration` is a private, temporary switch. Normal builds compile every exploration hook out. The second command links the production app and checks that exploration symbols are absent, reusing native objects from the shot build.

All gradients use existing dithered shaders. No icons were added. Animations remain enabled. The screenshot's first four title identities, order, Sopranos artwork, rating, and visible synopsis wording are preserved. The eight unseen titles use documented fixture order. A24 is a context-only second-row fixture. Quick settings and clock are omitted from native comparisons so they do not cover the row; the original reference remains available.

Limits: desktop native OpenGL at a TV 1080p canvas, not a device performance test. Exploration drawing does not finalize end-of-row scrolling, touch handling, long-title/missing-art fallbacks, reduced-motion behavior, or integration with the focused-card context overlay. Those belong to the chosen implementation after owner selection.
