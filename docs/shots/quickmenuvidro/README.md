# Quick-menu focus frost

The real `central.c` tile renderer now draws `gfx_vidro_folha` for focus even when interface glass is off. The plain fill fades out, active tiles retain 92% accent opacity, and the existing focus ring remains. Bounds and layout are unchanged. Frost uses the existing shared backdrop/cache.

`before/` contains the original renderer; `after/` contains the patched renderer. Captures use the real menu body over a deterministic offline background. The teal captures explicitly select the Petroleum accent; the other pairs use white.

Validation from the worktree root:

- Before patch: `bash tests/quickmenuvidro.sh docs/shots/quickmenuvidro/before` returned 1. Focused, active-focused and Edit tiles failed the frost-material assertion with interface glass off and on.
- After patch: `bash tests/quickmenuvidro.sh` returned 0. Focus material, unchanged tile bounds, teal chroma, no frost for unfocused tiles, and zero captures/blur passes for the settled menu passed.
- `NUVIO_MAC_SO_COMPILAR=1 NUVIO_MAC_BIN=/tmp/nv-205-quickmenuvidro-mac bash tools/mac.sh` returned 0. Existing warnings: incomplete local server configuration and deprecated `ATOMIC_VAR_INIT` in `src/perfis.c`.
- `git diff --check` returned 0.

No TV run, push, deployment or installation performed.
