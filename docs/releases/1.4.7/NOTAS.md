Continue Watching sort modes, an Upcoming row with air dates, smoother GIFs on Samsung, and home rows that stay the way you set them.

## Added

- **Continue Watching sort** (#127): Default, Streaming style or Split upcoming, in Settings. Episodes that haven't aired yet go to their own **Upcoming** row and show the date ("Airs Oct 21").

## Fixed

- **Hidden rows coming back**: rows you removed no longer return after a restart, disabled addons no longer create rows, and a new addon only gets a guaranteed spot when the profile has no order of its own.
- **Upcoming episodes cut off** a full Continue Watching row, and the same row showing twice.
- **Anime (ASS) subtitles with Real-Debrid** (#92): when the server cuts a range in the middle, the app keeps what arrived and asks for the rest, instead of retrying the whole thing forever.
- **Samsung GIFs** (#84): decoded natively off the main thread, so they animate smoothly and don't slow down navigation.
- **Menu sidebar** feels faster: the highlight fades out instead of sliding a gray block behind the focus.

## Notes

- Samsung: the package is unsigned on purpose; sign it with your own certificate as before.
- LG with the Homebrew Channel: add `https://github.com/iqui27/nuvio-native-legacy/releases/latest/download/repo.json` as a repository to get updates from the channel.

## Packages
- `space.nuvio.native.legacy_1.4.7_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.7_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.7-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
- `repo.json`, `webosbrew.manifest.json` — Homebrew Channel repository files.
