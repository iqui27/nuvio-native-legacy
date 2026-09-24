Posters with the right art again, Trakt rows on screen in seconds, a lighter Saved panel, anime subtitles that keep trying, and many fixes from your reports.

## Fixed

- **Cards showing another title's art** (e.g. Resident Evil with the Searching logo): a texture slot that was abandoned kept the previous image's bytes, and the next poster that took the slot used them.
- **Trakt rows late**: watchlist and collection now show as soon as Trakt answers, instead of after every addon catalog. A home load that was started with the wrong addon list stops at once instead of running to the end.
- **Home rebuilt from scratch**: collections, catalog order or row settings arriving during the home load no longer throw away what was already fetched.
- **Saved panel slow** (LG): the home is frozen behind the panel while it's open, posters are loaded at the size they're drawn, and the list is only rebuilt when something changes. 52 → 60 fps on a C9.
- **Menu leaving a trail** of highlighted items when moving fast.
- **Duplicate titles in Saved** (same show twice, same episode).
- **Library stopped at 205 titles**: account library now loads up to 500, the TV list up to 2000, and new titles never push out silently.
- **Missing catalogs from big addons** (#126): addons with more than 32 catalogs (like Ultra MAX) keep the ones you chose first, and all of them can be picked in Settings › Home rows.
- **Anime (ASS) subtitles** (#92): network hiccups no longer hand the subtitles to the TV for the whole episode; the app keeps retrying and takes them back. Long debrid URLs no longer break subtitle reading. The log now says which request failed and why.
- **"More like this" at the start of a movie** (#115).
- **Rows remembering the column after a restart** (#95).
- **Blur unwatched episodes** did nothing (#133); it now works on the detail page, the player's episode list and the watched menu.
- **Update card on Samsung**: the buttons are always visible and the notes scroll.
- **Samsung trailers** (#136): trailers now try Apple, then IMDb, then YouTube, and move on when one fails instead of showing YouTube's error screen or leaving the app. The home banner no longer opens YouTube on Samsung.
- **Slow Apple TV art / stalled downloads**: idle connections are dropped, a stalled transfer is cut and retried once on a new connection.
- **Samsung**: GIFs are downloaded once and animate slower while art is loading.

## Notes

- Samsung: the package is unsigned on purpose; sign it with your own certificate as before.
- LG with the Homebrew Channel: add `https://github.com/iqui27/nuvio-native-legacy/releases/latest/download/repo.json` as a repository to get updates from the channel.

## Packages
- `space.nuvio.native.legacy_1.4.6_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.6_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.6-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
- `repo.json`, `webosbrew.manifest.json` — Homebrew Channel repository files.
