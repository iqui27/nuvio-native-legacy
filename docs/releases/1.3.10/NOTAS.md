Samsung fixes from the 1.3.8/1.3.9 logs: settings that reverted on the next start now persist, animated GIF covers get their own Worker (they were queueing behind image decodes — 2 to 10 s per frame on 2 GB TVs), and trailer auto-play is off by default as intended (1.3.9 shipped it on). On LG: the TV guide no longer freezes for up to 3 s when a channel opens, and the title page goes into a cinema mode while the trailer plays.

## Samsung

- **Settings not saved between sessions** (#85). The settings file and the player prefs (aspect, subtitle style) are written to the app's virtual filesystem, which only reaches the TV's storage when a module flags a change — and these two writers never did. They persisted only when something else (progress, rows) happened to flush. Fixed: both flag the change.
- **GIF covers crawling** (#84, and the 9 s stalls in #85's logs). In 1.3.8 the GIF frames were composed in the same Worker that decodes JPEG/WebP, and that Worker blocks for up to 8 s per image while waiting for the app to allocate the pixel block. With many images arriving, GIF frames queued behind those waits: measured 200 s and 845 s for one loop of 90 frames. GIFs now have their own Worker that never waits on anything.
- **Trailer auto-play was on by default in 1.3.9.** The published `.wgt` came from the build used for the release screenshots, with auto-play forced on. 1.3.10 turns both switches (title page and home hero) off once, on first start; turn them on in Settings › Details / Home if you want them, and the choice sticks.
- **Pixelated hero for some Continue Watching titles** (#85): the hero used the episode still even when the source only had it at ~400 px. Under 900 px it now falls back to the title's backdrop.

## LG

- **TV guide froze for up to 3 s when opening a channel.** The playlist probe of the channel's sources ran on the drawing thread and waited for its timeout. It runs in its own thread now, like the movie source check. (If a channel then stutters or plays fast, the log shows the add-on's proxy timing out and the stream buffering — that is the source, not the app.)
- **Cinema mode on the title page.** When the trailer starts, the text block slides down and fades, the vignette drops to 15%, and only the title logo stays, small, at the bottom-left. Any key brings the block back (trailer keeps playing); Back closes the trailer and stays on the page.
- The "next episode" line on the title page is always white (it was in the accent color, unreadable over bright artwork); the armed reminder button is a white ring on the dark circle instead of the green disc; the sidebar's focused item uses the ink that contrasts with your accent color (it was always dark, black on pink).
- Source add-ons no longer come up empty when a title is opened in the first seconds after launch (the search is repeated when the account's add-on list arrives).

## Packages
- `space.nuvio.native.legacy_1.3.10_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.10_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.10-tizen.wgt` — Samsung Tizen (WASM).
