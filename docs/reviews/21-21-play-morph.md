# Restore the TV Play opening morph

Restored the Detail-to-player geometry handoff in three lines in `src/detail.c`. The current TV Play rectangle and visible title-logo rectangle now feed the existing `player_morph_de` renderer. No animation, spinner, layout, opening preference, or playback backend was replaced.

History and branch reconciliation, completed before editing:

- `718e61abb` introduced the exact owner-selected A3 morph: 900 ms ease-out-expo, Play pill becoming the ring below the title logo. `922500ef4` added the B3 corner targets.
- `dfa1b944d` brought the handoff into 2.1; merge `64c1302cf` removed the Detail geometry and call while retaining the player renderer.
- Only `agente/21-21-playmorph` matches this restoration. Related `agente/21-21-playinfo` and `agente/21-21-vodcarga` are already ancestors of base `fab11c7bf`; neither supplies a missing restoration. No branch was merged, deleted, or rewritten.
- Inspected `src/video*.c`, `src/abertura.c`, `src/ajcena.inc`, opening/loading renderers and existing animation/loading tests. The movie morph belongs to `src/player.c`, not the app splash or generated Settings previews.

Validation:

- `bash tests/play_morph.sh`: baseline failed at the real Detail handoff assertion, rc 134; restored tree passed, rc 0. One target test uses real Detail/player code, native GL rendering, a controlled clock and the existing Mac video simulation.
- Temporal assertions cover both A3/B3 styles, initial Play geometry, shrinking at 150 ms, final ring at 900 ms, rotating indeterminate loading, measured buffer, ready-before-first-frame hold, successful reveal, reduced motion, error, cancel, synthetic Back, Detailed opening and stale handoff fallback. No D-pad or device input was sent.
- `bash tools/build-local.sh`: rc 0. Build output isolated under `/Volumes/ExternalSSD/tmp/rev/j-21-play-morph/build`; existing atomic-initializer deprecation warnings remain.
- Eight native 1920x1080 PNG frames and two inspected JPG contact sheets: `/Volumes/ExternalSSD/tmp/rev/j-21-play-morph/`. Each style has `0-play`, `1-morph-150ms`, `2-loading`, `3-video-fixture`. The last frame uses cached artwork as the existing capture hook's substitute for the hardware video plane; actual TV decoding and owner acceptance remain untested.

base-dono: existing TV Detail and owner-selected Center A3 / Corner B3, preserved from their historical implementation. Read `base-dono.md`; no new visual language, Sketch edits, board corrections or layout changes.

The requested `.claude/tv-design-system-sketch.md` is absent from this worktree, searched local references and Git history. Read linked AGENTS, CLAUDE, DECISOES, DESIGN, MOCKUPS and PRODUCT instead. Owner TV testing remains pending. No push, deploy, release, merge or issue closure.
