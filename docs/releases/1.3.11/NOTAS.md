Samsung: the trailer switches now stay the way you set them and the title page plays the Apple trailer instead of the YouTube embed that failed on the TV (#82, #86). Everywhere: text over your accent color is white unless the accent is white, the sidebar opens from any screen with ←, the Schedule gets a hold-OK menu with the latest news about each show, and the TV guide header is one ← away.

## About trailers — there is no trailer button

Trailers play **on their own**: open a title and leave the remote alone for a couple of seconds and the trailer starts in the background, muted, behind the artwork; on the home screen the same happens when the focus rests on the top hero. Press OK while it plays for full screen with sound; any other key brings the page back; Back closes it. Both are switches in Settings › Details ("Trailer autoplay") and Settings › Home ("Trailer on the hero"). On Samsung both start **off**.

## Samsung

- **Trailer switches reverted every time you opened Settings** (#82, #86). Opening the Settings screen re-applied the factory "off" to the two trailer switches on top of what you had saved, and the next save wrote that back to disk. Removed; the one-time reset that 1.3.10 does for installs coming from 1.3.9 still runs once.
- **"Video player configuration error" when the trailer started** (#82, #86). That message is YouTube's: the embed refuses to play from a TV app that sends no Referer. The title page now plays the Apple TV trailer (HLS, native `<video>`, like the home hero already did) and only falls back to YouTube for titles Apple does not have.

## Everywhere

- **Accent color and text.** Text on a surface painted in your accent color is white — unless the accent itself is white (or nearly), where it stays dark. The old rule guessed by luminance and put dark text on pink and yellow in Settings, Notices, Schedule, TV guide tabs, add-ons, source pills, "View title" on the hero. The armed reminder button on the title page is a filled disc in the accent color, no ring.
- **Sidebar from any screen.** ← with nowhere left to go leaves the screen and the home comes back with the sidebar already open: Schedule, Library, Search, Profile, Add-ons, Settings (categories column), Social. Not from the TV guide, and not while the live PiP is open.
- **TV guide:** ← on the first column (cards) or with the time window already at "now" (list) jumps to the Cards / List / Add-ons header; ↓ returns to the row you were on.
- **Schedule: hold OK on a show** for a menu — open the title, latest news, reminder on/off. A short press still toggles the reminder. **Latest news** lists headlines about the show (Google News, in the app's language, newest first, with outlet and date); the focused row shows the newest one as a quote under "Last episode on …" with "Hold OK to read more". No links — a TV cannot open them — headlines only, cached 6 h.
- **Title page logo.** The written title no longer flashes before the logo replaces it: the box stays empty while the logo loads and the logo fades in; the written name is only the fallback for titles without a logo.
- **Poster focus ring corners** on the home were sharper outside than inside (the outer radius was the poster's); fixed.
- **Network threads that started while the first one was still loading libcurl came back as "no network"** — measured with seven news requests starting together: one worked, five failed instantly. Same pattern as the four artwork threads at launch on LG. Fixed: they wait instead.

## Notes

- Samsung: the news request has no CORS header; it relies on the app package's `access origin="*"`. Not verified on a Samsung yet — if the news panel says "Nothing published recently" for every show, that is why; please report.
- Samsung: the Apple trailer starts at a low resolution and adapts (HLS); LG picks the best single variant. Improving the start on Samsung is next.

## Packages
- `space.nuvio.native.legacy_1.3.11_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.11_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.11-tizen.wgt` — Samsung Tizen (WASM).
