# Sports event details and frame cost

Run `bash tests/espevento.sh` from the worktree root. It uses the production SDL/GL renderer, guide input dispatcher, reminder storage, and an offline 400-event catalogue. The target also runs the existing sports parser/state/order/UI, guide, EPG lifetime, reminder and sports artwork checks. `ESPEVENTO_PROOF=before bash tests/espevento.sh` selects the original guide/card renderers from commit `3485af0f0` with the same fixture and host renderer.

Artifacts are written to `/Volumes/ExternalSSD/tmp/rev/espevento/`: `before-` and `after-` PNGs for sports home, live grid, results, live details, unavailable live details and upcoming details; `before-perf.log` and `after-perf.log` contain measurements.

## Method and observed results

Apple M4 Pro, OpenGL 2.1 Metal, 1920 × 1080, solid material. Each page warms for 30 frames and measures 120 frames. Draw time includes `glFinish`, excludes window swap; update measures the guide input/hold tick separately. It does not benchmark the network worker or the full application update loop. The home collection contains 400 events; current date filters produce 113 live/today and 56 finished events. Three game cards are visible, matching production pagination. Animations remain enabled. Times vary with other host jobs.

| Page | Before draw mean | After draw mean | Rects before / after | Text draws before / after |
| --- | ---: | ---: | ---: | ---: |
| Sports home | 0.755 ms | 0.678 ms | 98 / 86 | 37 / 25 |
| Live/today grid | 0.692 ms | 0.494 ms | 103 / 52 | 50 / 23 |
| Results grid | 0.742 ms | 0.686 ms | 103 / 52 | 50 / 23 |

Final measured update mean: 0.000–0.001 ms. Warm text rasterizations/uploads and artwork uploads: zero in both versions. Worst after draw: 3.682 / 3.066 / 4.797 ms respectively. These host results establish a reduction in submitted work, not 60 FPS on TCL. The supplied TCL log reported approximately 30 FPS and 24–40 ms draw before this change; no after measurement was taken on TCL because deployment is outside this job.

## Implementation and boundaries

Visible game cards retain their rendered pixels in six bounded FBO slots, invalidated by metadata revision, event contents, geometry/scale, language, artwork quality, material, clock format and focus. Pending artwork/text retries are throttled to 250 ms. Section labels retain six transparent surfaces, leaving material and animated focus live. Section event counts/previews are computed when the collection changes. Live channel filtering is cached by guide revision, profile, query and a two-second refresh. Shared text and artwork caches still own transient resource lifetimes. A new `gfx_n_text` counter counts text texture submissions.

Event details use the existing guide page transition and focus navigation. Event/broadcaster names stay provider text. Watch links accept confirmed EPG or explicit broadcaster matches and respect provider/category/parental filters. The existing Sources matcher remains stricter: it requires confirmed EPG. A missing watch link retains teams, score/status, competition, venue/city and local kickoff. Reminders use the existing profile storage and `guialembrete.c` notification path; a channel-independent reminder expires as a notification without a fake playable channel. Upcoming details focus Remind me. Result cards use parsed team colours, neutral fallback and numeric winner emphasis.

Following is an integration slot only: espseguir can call `g5ui_sports_follow(action, state)` from `guiav5ui.h`, passing its `esportes_seguir_*` adapters. No registration means no Follow control.

The offline event regression checks short OK release, live preview dispatch, explicit broadcaster links without confirming EPG, unavailable events with monogram fallback, upcoming primary focus, reminder save/reload/remove and Back focus restoration. It asserts mean draw below 16 ms and zero warm text uploads. It does not claim real stream decoding or reminder delivery on a physical TV.
