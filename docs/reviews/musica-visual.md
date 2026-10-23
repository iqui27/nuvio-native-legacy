# Soundtrack mockup correction

Mockup only. Source and rendered assets remain in `/Volumes/ExternalSSD/tmp/musica-visual/`; no native app or worker changes.

`src/musica.html` hard-codes pt-BR UI, including save confirmation, preview state, pagination, accessibility labels and remote hints. Track/album names and cover artwork remain original. The explicit pt-BR mockup request takes precedence over shared translation-table guidance; reports remain English.

Background reuses each album cover with static CSS blur, saturation and dimming instead of averaging its colors. The Dark Knight retains blue and fire-orange regions; Game of Thrones uses its monochrome cover palette.

Validation from worktree root:

- `bash tests/musica_visual.sh`: pt-BR UI assertions, per-cover background, layout, D-pad paging, save, OK/Back stop, preview time limit and error cleanup passed.
- `bash tests/musica_visual.sh --render`: same checks plus four PNGs and six-second MP4 render passed.
- Final four PNGs opened individually: no English UI; original titles, artwork and Apple Music branding retained.
- MP4 inspected with ffprobe: H.264, 1920 × 1080, 30 fps, 180 frames, 6 seconds.

External files changed: `src/musica.html`, `render.cjs`, `README.md`, `musica-filme.png`, `musica-foco-faixa.png`, `musica-serie.png`, `musica-guardado.png`, `musica-anim.mp4`, `musica-anim-check.png`.

External source SHA-256:

- `src/musica.html`: `58f4477a5d44b1fce944599f77c7110e7146092dfe57f91b6fb20742a31d40bc`
- `render.cjs`: `45443ecf0932ade679cee953eb927d0171618380050be0aab45df31314129098`

No push, deploy or stash. Requested mockup correction has no pending work.
