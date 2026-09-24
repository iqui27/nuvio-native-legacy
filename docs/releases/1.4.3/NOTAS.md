Fixes for the problems 1.4.2 introduced on Samsung — the Diagnostic freezing, slower artwork, a log prompt on every start — plus hero art from the wrong title, embedded subtitles on Samsung, and anime pages showing another title. If 1.4.2 froze on the Diagnostic, **please run it again now: Settings › Diagnostics › Diagnostics and optimization.**

## Fixed

- **Diagnostic froze on Samsung** (#113): pressing OK locked the app — a file lock was taken twice on the same thread (only Samsung has that lock, so LG was fine). The same mistake was in six other places, including app start.
- **Artwork slower on Samsung since 1.4.2** (#114): the new persistent art cache asked the page's main thread before every image and waited up to 1.5 s each time (median download went from 353 ms to 1.6 s). On Samsung art goes straight to the network again, with the browser's own cache.
- **Hero showing the previous title's art** (#118): two art lookups shared one buffer, so the second overwrote the first. On Continue watching it could stay on "Loading artwork…" forever.
- **Cards showing another title's art** (LG, #116): posters with a neighbour's backdrop, a Continue watching card with another show's episode still. Since 1.4.2 the app reused connections, and a transfer that was cut short could leave bytes behind for the next image. Now only clean transfers reuse a connection, a finished image is checked against the request before it goes on screen, and the art cache is cleared once on the first start of 1.4.3 so wrong images saved by 1.4.2 don't come back. Expect the first screens to load art again once.
- **"Send log" prompt on every start** (Samsung, #120): the app closed before the "closed normally" mark was saved, so every start looked like a crash.
- **Left/right seek** (#121): with the controls hidden, left/right now seek right away on the progress bar; press down for the buttons.
- **Embedded subtitles invisible on Samsung** (#122): the TV hands the subtitle text to the app instead of drawing it, and the app was dropping it. It is now drawn with your subtitle style.
- **Anime subtitles** (#92): the app could hand an ASS track back to the TV silently (before reading the file index, when the track list was longer than 12, or when the file has no subtitle index). Now the app keeps it, reads files without an index in a window around playback (never the whole file at once), retries short network failures instead of giving up, and logs the reason whenever it falls back to the TV.
- **Anime pages showing another title** (Mushoku Tensei): anime catalogs mark items as "anime", and the app treated that as a movie — it asked for the movie with that id and got a 1965 film with no episodes. Now it asks for the series first.
- **Episode thumbnails missing after season 1** (One Piece and other long shows): the metadata and TMDB split seasons differently, so "season 3, episode 1" didn't exist on TMDB. Episodes are now matched by air date, and a card without a thumbnail shows the show's backdrop instead of staying empty.
- **Series trailers** (#123): TV shows now get the Trailers row and hero trailers like movies, and trailers in English or without a language are no longer filtered out when the app is in another language.
- **Samsung Tizen 5.5:** the app no longer stops at start on browsers without `globalThis`.

## Changed

- No "send log" prompt when the app starts after a crash; the crash stays in Alerts. Automatic log sending keeps working for those who turned it on.
- Focused trailers are highlighted; the "Recommend to a friend" sheet uses the same focus style as the other menus; filled bell icon in the Alerts toast; the LG blue key is drawn with the shape of the actual button.

## Notes

- Samsung: the package is unsigned on purpose — a distributor certificate locks the install to a fixed list of TVs. Sign it with your own certificate as before.
- If subtitles still fail, send a log right after choosing the track: the lines starting with `[legenda]`, `[mkv]` and `[mkvass]` say exactly why.

## Packages
- `space.nuvio.native.legacy_1.4.3_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.3_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.3-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
