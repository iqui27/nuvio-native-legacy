Friends get faces, the title logo stops being downloaded three times, you can dim (or remove) the dark layer over the title page, add-on posters that fail get a backup, and the Samsung log now measures the one thing left: how long a seek blocks.

## Added

### Friends with photos
![Friends in the Social tab](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.7/friends.jpg)

The SOCIAL tab lists your contacts under **Your friends** — photo (or initial), name, and for Trakt friends what they last watched (`watched · Silo S2E6`), taken from the same activity feed as the "Friends watching" row. The "Add a friend" and "To whom?" sheets show the photo on each row and separate the already-added friends under their own heading.

### Backdrop dimming (Settings › Title page)
0–100 % in steps of 10; 100 is the vignette the app always had, 0 shows the artwork clean. Local to the TV, not synced to the account.

## Fixed

- **Title logo slow to appear on the detail page.** The open card asked for the logo at `w500`, the hero and the detail page at `w1280` — three files of the same image, and opening a title downloaded the big one from scratch even with the small one already on screen. One URL for all three now (`w1280`; `w500` on the Low quality setting): the first place to show it downloads once, the others reuse the file. The detail page also shows the smaller decode immediately and swaps to the sharp one when it is ready, instead of a gap.
- **Posters missing in the library** (#67, add-on hosts). The TMDB backup added in 1.3.4 only covered `images.metahub.space`, the one host whose URL carries the IMDb id. The catalog now records which title every add-on poster belongs to, so a poster from any host that answers 404 (bingecat.com in the report) falls back to TMDB by IMDb id.
- The "NOTICES" tab is called **ALERTS** in English.

## Samsung

- **Seek timing in the log.** The automatic logs from 1.3.6 show that on Samsung the worst frames — 1 to 8.5 s with the main thread stopped — all come right after a seek, inside the player's `seekTo()`. This release logs `[video] seekTo bloqueou N ms` around that call so the next fix is aimed at the right thing (pause → seek → play, or seeking outside the frame) instead of guessed. Everything else on Samsung is unchanged from 1.3.6, which the AU7000 reporter calls "perfect".

## Packages
- `space.nuvio.native.legacy_1.3.7_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.7_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM (capped by the TV's RAM).
- `NuvioTV-1.3.7-tizen.wgt` — Samsung Tizen (WASM).
