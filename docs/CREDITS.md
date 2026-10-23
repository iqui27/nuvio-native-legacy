# Credits for third-party code

Nuvio Legacy is GPL-3.0. Code adapted from other projects keeps its original
copyright, noted here and in a header comment at the adapted code.

## PLX Native

- Source: https://github.com/GLinnik21/plx-native, commit 9d2777fe
- License: GPL-3.0-or-later
- Copyright (c) 2026 Gleb Linnik and contributors
- Adapted in `src/gfx.c` (glass pipeline: reduced-resolution backdrop blur,
  Kawase taps, `fs_glass` shading, lowbias32 dither texture) from
  `rust-modules/gfx/src/gfx.rs`, `shaders/fs_blur.frag` and
  `shaders/fs_glass.frag`. See `docs/issues/vidro-plx.md`.
- Adapted in `src/menu.c` (top bar: tab strip geometry, travelling selection
  and focus capsules) from `rust-modules/ui/src/widgets.rs` (`draw_tab_row`,
  `TabStrip`, `Capsule`) and its geometry/motion tests.
