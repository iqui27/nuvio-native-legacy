Samsung no longer freezes while the home loads (#72, #77, confirmed on an AU7000 with the rc1), the card that opens on the home tells you enough to decide, Settings are one category per page, every focused button uses your accent color, and long shows finally show all their seasons (#79).

## Fixed

- **Samsung: freezes and stutters while rows load** (#72, #77). Four causes, all removed: images are decoded in software already scaled to the size shown (JPEG via libjpeg with `scale_denom`, WebP in a browser Worker), no temporary file per image, no per-frame `stat()` on `/tmp` probes (that was 8% of the main thread in `FS.ErrnoError`), and card requests ask metahub for `background/small` instead of a 600 KB `medium`. Emscripten `ASSERTIONS` are off in release builds. rawldon on the rc1: "Seems to be fixed now... image and backdrop seem to be loading faster now."
- **Shows with more than 12 seasons stopped at season 12** (#79, South Park, The Simpsons). The season list held 12 entries; it holds 64 now. The ratings tab's season strip scrolls when the seasons do not fit.
- **Trakt comments: the "TV Show" cards could not be reached** (#78). The selector and the cards share one row, so reaching the show's cards meant passing over the "Episode" pill, which switched the source on the way. The pill now switches on OK, not on focus. The episode pill also says which episode it is showing (`Episode · S4E1`), and the cards follow the last episode you focused in the row above, not the resume target.
  ![Comments selector](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.4/comments-episode.jpg)
- **Search results from a TMDB add-on opened empty** (no seasons, no episodes, no sources). Their id is `tmdb:<n>`, which Cinemeta answers with 404. Such items are now resolved through TMDB's `external_ids` to the IMDb id before the page opens, the same path the cast filmography already used.
- **Open card on the home overlapped its right neighbour.** The push applied to the neighbours ignored the focus scale of the open card.
- **Watched tick on cards** is the check icon (#74). **Collection page tabs** use the add-on manifest's catalog name (#76). **Artwork backup** from TMDB no longer depends on the "TMDB" setting being on (#67).
- An episode without comments (or a failed request) was re-requested every frame while the section had focus.
- Chart tabs follow the season chosen in the Ratings tab and switch immediately with left/right.

## Added

### The open card says more
![Open card](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.4/open-card.jpg)

Resting on a poster opens it into the wide art, as before. The open card now carries, opposite the logo: the IMDb badge and rating, year and season count, the age rating, and a trend chip — `↑ 3`, `↓ 2` or `New` — for how the title moved in that row since the last day you opened the app. The chip only appears once there is a previous day to compare against; nothing is invented. Titles in progress get a thin progress line at the base.

### Every focused button in your accent color
![Hero buttons with the Violet accent](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.4/hero-violet.jpg)

The detail page buttons (play, add, watched, sources, reminder, share), the info tabs, comment cards, context menu, Saved panel rows, Agenda, update card, recommendation flows, the episodes panel and the What's new cards were still white on focus. They use the accent from Settings now, with the text black or white depending on the accent's brightness.

### Settings, one category per page
Categories open one at a time with a slide, rows are taller and grouped, every row has a family icon, and the help panel draws previews where words fall short. **About › Send log** uploads this session's log on demand (LG: the log file; Samsung: the `nv-log` kept in localStorage), so you no longer need a crash to send one.

### Notices
The toast lasts 20 s, sits top-right, breathes in the accent color, and draws the key that opens it (the blue button on LG, CH+ on Samsung) instead of naming it. A maintainer notice grows to its full text when focused in the list. The NOTICES tab badge is a small count inside the pill.

### Hero
"Continue where you left off · N min" and "Up next" are told apart, both with `S1 E4 · episode name`.

### What's new card
One page, shown once.

## Notes

- Samsung: the fixes were verified in a real Chromium with the same COOP/COEP isolation the TV uses and confirmed on an AU7000 (#72). Please keep reporting; **About › Send log** is the fastest way.
- Trend chips need two different days of use before they appear.
- Voice search is not possible in a native app: the Magic Remote and Bixby microphones only type into web text fields.

## Packages
- `space.nuvio.native.legacy_1.3.4_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.4_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM (capped by the TV's RAM).
- `NuvioTV-1.3.4-tizen.wgt` — Samsung Tizen (WASM).
