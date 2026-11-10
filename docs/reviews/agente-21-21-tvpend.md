Screensaver protection can remain unavailable after a registration failure and can suppress the screensaver for background trailers. The new library screenshot test also aborts before completing its captures.

Full review comments:

- [P1] Retry failed screensaver subscriptions — /Volumes/ExternalSSD/nv-21-21-tvpend/src/video.c:1207-1209
  If registration fails during initialization, the app never retries: `protegerScreensaver()` is called only from `iniciar()`, which returns immediately on later calls once `ligado` is set. Also, a successful `LSCall` only means the request was sent; an asynchronous error response is ignored by the callback. A transient failure can therefore leave the screensaver active during every subsequent film until the app restarts. Handle the subscription response and retry unsuccessful registrations.

- [P2] Exclude background trailers from screensaver inhibition — /Volumes/ExternalSSD/nv-21-21-tvpend/src/video.c:1193-1195
  When an automatic home-screen trailer is playing, `trailer_abrir()` uses the same `video_tocar()` pipeline, so these flags also identify the muted background preview as a film. A screensaver request arriving during that preview is rejected even though the user has left the app idle, potentially keeping a static TV interface on screen. Distinguish actual player playback from background trailers before sending `ack:false`.

- [P2] Limit the pixel assertion to the outgoing row — /Volumes/ExternalSSD/nv-21-21-tvpend/tests/biblioteca_shot.c:338-342
  The new pixel check counts every bright scanline from `y0` to the bottom of the screen, including the focused row rather than just the outgoing row. Running `bash tests/biblioteca_shot.sh /tmp/nuvio-review-shot` aborts at this assertion on the first pass, preventing the remaining library captures from being generated. Restrict the measurement to the outgoing row's viewport region so it both passes on the fixed rendering and detects a missing row.

## Follow-up (fix P1/P2 review findings)

**P1 — screensaver subscription retry (`src/video.c`)**
`protetorLigado` now means hub-confirmed subscribe, not `LSCall` queued. `protetorPendente` + `protetorGeracao` block duplicate registers and ignore stale callbacks after `video_encerrar()`. Immediate and async failures use `LsRegEstado` backoff; `video_bombear()` calls `protegerScreensaver()` when retry is due.

**P2 — home hero preview (`trailer.c`, `home.c`, `lsregistro.c`)**
Home autoplay uses `trailer_abrir_preview_home()`. `video_segura_screensaver(..., previewHome)` releases hold for that path; full-screen trailers and foreground playback unchanged.

**P2 — library shot assert (`tests/biblioteca_shot.c`)**
Pixel check scans only the outgoing row-0 band in the grade viewport (scroll math from layout constants). Failures set `pixelFalhou` and remaining BMPs still capture; `assert(!pixelFalhou)` runs at exit. Negative control (force `nVis > 99999`): run reached `pronto` then failed at final assert (abort), proving the guard is live.

**Tests**
- `bash tests/lsregistro.sh` → exit **0**
- `bash tests/biblioteca_shot.sh` → exit **0**

**Limits:** No webOS TV run for real `registerScreenSaverRequest` / idle screensaver timing. Detail-page background autoplay (`cheia=0`, not home preview) still uses the normal hold predicate. No commits in this pass.
