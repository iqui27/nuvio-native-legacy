Trailers play inside the app: on the title page a few seconds after you open it, and on the home hero when the focus rests there. On LG the trailer comes from Apple TV (up to 4K HEVC) with the IMDb trailer as a backup; on Samsung, from the embedded YouTube player. OK on a trailer opens it full screen with sound — no more being thrown into the TV browser (#82). Also: source add-ons no longer come up empty when you open a title in the first seconds after launch, and a JSON bug that froze Samsung TVs for up to 12 s while applying account collections is gone.

![What's new card](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.9/whats-new.jpg)

## Added

### Trailer on the title page
![Trailer playing behind the title page](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.9/trailer-title.jpg)

About 2.5 s after the page settles, the trailer starts muted in place of the backdrop, with the same dark vignette so the text stays readable. Scrolling down, opening a cast page or a menu, or leaving the page brings the art back. One attempt per visit: a trailer that ended does not restart.

### Trailer on the home hero
![Trailer on the home hero](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.9/trailer-hero.jpg)

With the focus resting on the hero for 4 s and the art settled, the title's trailer plays in the hero area — with no overlay on top, the art's gradient comes back with the art. Moving the focus, or anything opening over the home, stops it. The carousel waits for the trailer to end before advancing.

### Full screen with sound
OK on a trailer card (or on the trailer button) plays it full screen, with sound, inside the app. OK pauses and resumes; Back closes. This is the fix for #82: the trailer used to open the TV browser, and there was no way back.

### Where the trailers come from
- **LG**: the Apple TV app's own trailer for the title — a clean HLS ladder, HEVC up to 3840 wide, matted to the film's aspect (no baked-in black bars). Found by exact title + year; if Apple has no confident match, the IMDb trailer (MP4 up to 1080p) is used. The app plays a single variant chosen by your quality setting, not the adaptive ladder, so the picture stays at one size and the crop stays right.
- **Samsung**: the embedded YouTube player, from the same trailer list the title page already shows (TMDB). The home hero uses Apple's HLS in a `<video>` element.
- YouTube resolvers were evaluated and rejected: every public Piped/Invidious instance was down or bot-blocked, cobalt requires a key, and yt-dlp from a server IP gets "sign in to confirm you're not a bot".

### Settings
- **Details › Trailer auto-play**: the title page trailer, on or off.
- **Home › Trailer on the hero**: the hero trailer, on or off.
- **Details › Trailer quality**: Maximum (the best the source has), 1080p, 720p or 480p — a ceiling.
- **Details › Trailer aspect**: Cinema zoom (default; removes the letterbox of a widescreen trailer), Slight zoom, Ultra zoom or Original. Same zooms as the player.
- On **Samsung**, both auto-play switches ship **off** by default: one change per build on that platform, and the trailer button already works in-app. Turn them on in Settings if you want them.

## Fixed

- **Sources empty when opening a title right after launch.** The search ran against the add-on list from the package before the account list arrived (about 5 s in), and nobody repeated it. The title page now re-runs the source search when the add-on list changes.
- **Samsung: up to 12 s frozen while applying account collections** (seen on a Tizen 9 TV with 256 collection folders; 1.2 s with 157). The JSON key lookup scanned to the end of the whole blob for every absent key. It is bounded now: 283 ms → 6 ms for 256 folders on a desktop, proportionally on the TV. This is the same stall that appeared as "1 s when picking a profile" in earlier logs.
- **LG: source crop lost after the pipeline started playing.** Two places re-applied the plain display window on top of a source crop (the ACB bind and the `playing` event). They now re-apply the crop. This also makes the player's zoom modes survive those events.

## Under the hood

- `trailer.c` (both engines), `trailerimdb.c` (IMDb GraphQL, with headers the browser cannot send — LG only), `trailerapple.c` (UTS search + movie/show payload + single-variant master written to disk), `video_volume`, `video_terminou`, `video_recorte_reaplicar`, `addons_versao`.
- On this LG (C9, webOS 4) the ACB source crop is only honoured when requested after `playing`; requested earlier it is accepted and ignored. The trailer sends the full frame first and the crop 0.8 s after playback starts.
- New tests: `tests/trailerimdb.sh`, `tests/trailerapple.sh` (both hit the network).

## Notes

- Apple's UTS API is undocumented and its `robots.txt` disallows it; this is a personal build and the decision to use it is the maintainer's. Removing `trailerapple.c` leaves the IMDb and YouTube paths intact.
- Samsung 1.3.8 logs: one Tizen 9 / 2 GB TV with 16 home rows and many GIF covers shows 9 s main-thread stalls in the frame swap and decode timeouts. Not addressed in this release; the GIF Worker and the row count are the suspects for the next Samsung-only build.
- The What's new card is shown once.

## Packages
- `space.nuvio.native.legacy_1.3.9_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.9_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.9-tizen.wgt` — Samsung Tizen (WASM).
