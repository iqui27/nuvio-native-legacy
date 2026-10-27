# Sources island restoration

Restores the 2.0.8 Sources renderer for every installed add-on, including AIOStreams. The 820 px body uses the clock island outside playback and the player island during playback. Quality groups, counts, white badges, size/provider columns, playing marker, provider tabs and focus use the existing renderer. Inspect title and Open dashboard remain header action discs, with translated focus help. Long media URLs, uncached-clip handling, server filtering and candidate caching remain intact.

The 2.1 automatic-choice card and folded source rows were removed from the sheet to restore the reference list. Playback selection and inspection APIs remain available.

## Regression history

- `10225ca74`: introduced `corpoAio`, the conditional 1336 px width and separate source presentation.
- `9a12afba8`: bypassed the clock island when AIOStreams was installed; added the floating left title/recommendation, bottom filter chips and Source order footer.
- `ec9083ebc`: merge explicitly retained the AIO layout over the shared sheet.
- `09381c7cb`, `b9d63e46a`: added/refined the separate automatic-choice card and folded rows, removed here for 2.0.8 layout parity.
- The task's later material/guidance commits (`c230a6410`, `3a41f8345`, `6b40819a5`, `0d567ec41`, `2b4ca5984`, `adb24f6fb`) did not introduce `corpoAio`. `fdc8c1d17` added server row/provider presentation, not the AIO layout split.

## Other surfaces audited

- Together Sources: `juntosui.c`, `pedirCorpo` uses `ilha_corpo` outside playback and `plrilha_pedir` during playback. It retains island morphing and owns a separate room/source layout.
- Servers Sources: `app.c` opens `stream_folha_abrir` then `stream_folha_servidor`; inherits this restoration. Its missing source-priority footer is restored with existing translated copy; provider filtering remains. Footer space reduces only the list viewport, preserving the full island surface bounds.
- Sources filter chips and provider tabs: part of the restored shared body, so they ride the island.
- Add-on guide: `addonguia.c` uses `plrilha_pedir` with `centro`, `modal` and an explicit center destination. This is a separate centered modal, not expansion of the resting clock island. Listed exception; does not reuse Sources.
- AIOStreams filters: `aiostreamsui.c` renders a full workspace page with vertical entry. It does not expand from the clock island and does not reuse Sources. Listed exception.

## Evidence and limits

`bash tests/fontes_restore_shot.sh` renders the tagged 2.0.8 `streams.c` and the fixed renderer against the same current native runtime. Compatibility adapters only satisfy new caller symbols in the reference build; they do not replace its renderer. Native frames are 1920x1080 with the rail visible at 85% and 100%, glass and solid. Geometry assertions verify first-frame growth from the clock, 820 px settled width, stable right edge and return to the pill. Captures also exercise header and provider-tab focus.

Evidence directory: `/Volumes/ExternalSSD/tmp/rev/21-21-fontes/`. `compare-{85,100}-{glass,solid}.png` shows reference left and fixed right; `motion-{85,100}-{glass,solid}.png` shows opening and closing. All 140 native/comparison PNGs were opened. Six contact sheets and the final Sources comparisons, motion strips and server footer were visually inspected.

The data reproduces the five Fallout sources from the repository's approved player screenshot fixture (9.4/18.2/7.8/2.1/1.8 GB, DV/HDR10/HDR10+/REMUX). These are offline reference fixtures, not a fresh live VOD response. The reference build shares current fonts/materials; this is renderer comparison, not a full historical binary. No TCL execution, deployment or push was performed.

Validation: `bash tests/streams_folha_cache.sh`, `bash tests/fontes_restore_shot.sh`, `bash tools/build-local.sh`, `python3 tools/varredura-i18n.py`, all exit 0. Final build includes the server-footer geometry fix; subsequent source edits only reuse existing translated footer strings. The capture harness tolerates real clock minute rollovers when checking the collapsed pill.

## Missing actions after Live TV

Audit baseline: `integracao/2.0.5` (the 2.0.8 line) and 2.1 through `5423a59d4`, including `75432c801`, `fd44f4b48` (chipfonte), `bc7ee6bf8` (chipfonte2), and `949ea5b51`/`a3eee0b65` (fontesanim). `ess-fontes` changed settings rather than sheet actions.

The retained source list could inherit the last Live TV request's `pedidoVod` and cached `naoVod` classification. The sheet then offered only Refresh and Close, hiding MP4, cache, audio language, AIO title inspection, and AIO dashboard. `stream_folha_item` now reconciles the scope with the actual catalog item and refreshes cached row scope when it changes. Existing Lucide icons, translated focus help, handlers, materials, and animation remain in use.

Both versions retain OK to play, Info/yellow/channel-down for source metadata, hold OK for supported file browsing, and conditional Force SDR/reload actions. No removed Play with, copy/share URL, download, trailer, or manual-sort action was found in the shared sheet. Track selection remains in the player; source sorting remains settings-driven. The separate AIO layout's order summary was removed by the previously approved shared-sheet restoration.

The animation regression now reproduces retained Live TV state before opening a series. It failed before the fix at the seven-action assertion, then checks action registration, source-row registration, filter toggles, refresh, Info/Back, and playback in warm, cold, full, reduced, and solid modes. The navigation clock fixture now advances the real island drawing on each simulated frame: the spring lives in drawing, so update-only settling previously left the reveal gate closed. Production reveal thresholds and spring timing are unchanged. Navigation timeouts allow the full native gate to complete on a loaded host.

Evidence: `/Volumes/ExternalSSD/tmp/rev/fontesbotoes/after/`. `compare-{85,100}-{glass,solid}.png` shows historical renderer, current failure, and fixed renderer, respectively. `motion-*` shows the original opening and reverse close. The harness preloads Lucide assets before sampling frames. Historical captures use branch `streams.c` with current native graphics and link-only compatibility adapters; these are offline fixtures, not full historical binaries or TCL runs.
