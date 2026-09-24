Profiles stay apart, the home opens in your order, one source per play, and your subtitle language finally sticks.

## Fixed

- **Profiles mixing** (multi-profile accounts): switching profile while the app was still syncing could put the previous profile's collections, library, watched items and Continue watching into the new one. A sync that started on another profile is now thrown away and run again for the right one.
- **Trakt and Simkl are now per profile**: the link used to belong to the whole TV, so every profile showed the same watchlist and Trakt rows. Your current link moves to profile 1; link other profiles from their own Settings.
- **Home rows per profile**: new profiles start in the automatic order instead of copying the TV's old choice.
- **Home order arriving late** (#125): the saved catalog order is applied as soon as the profile is known, before the account answers, so the home no longer reorders itself seconds later.
- **Subtitle/audio language not saved** (#129): opening Settings reset any language other than "From account". The preferred subtitle language now also turns on by itself when a video starts: embedded track first, then addon subtitles.
- **Several debrid files loaded per play** (#130): sources were checked 4 at a time before playing, and each check makes TorBox/Real-Debrid add the file. Now only one source is checked at a time, and it stops at the first that works.
- **Cards with broken art** (rare): the home could read a catalog entry that had just been freed.
- **Diagnostic flipping the profile on every run**: it now changes the profile only when the gain is clearly above network noise.

## Added

- **Automatic source** (Settings › Playback): "Best source" (as before) or "First in list", which plays the addon's first result, like Nuvio's "Auto-play first source".
- **Another source on failure**: Off, 1, 2 or 3 (default 2).

## Changed

- LG TVs with 1.2–2 GB of RAM get a 128 MB texture budget (was 96 MB).
- TMDB and metahub art is downloaded at the size it is drawn instead of 4K.
- Samsung: when the TV refuses a source, the log now says why.
- Smaller logs: playback clock events are sampled every 30 s, and Samsung sends its log every 5 minutes after the first 5.

## Notes

- Profile 1 keeps the Trakt/Simkl link this TV already had. Other profiles start unlinked.
- Samsung: the package is unsigned on purpose; sign it with your own certificate as before.

## Packages
- `space.nuvio.native.legacy_1.4.4_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.4_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.4-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
