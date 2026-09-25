A new live TV guide, Settings organized like the web app, matching color, a speed test, and artwork you can pick yourself.

## Added

- **Matching color** (Settings › Appearance › Accent color): the accent follows the title you're looking at. Four styles: Matching, Styled (the background takes a dark tint of the art), Gradient (buttons, focus ring and bars use two or three colors from the logo) and Immersive (the art's light spills into the screen). "Logo color" takes the colors from the title's logo and is on by default.
- **Live TV guide**: big preview with what's on now and next, full-width grid with a now line, real channel logos and a categories panel. Press ↓ or Blue during a channel for a mini guide over the video. Opening the guide keeps the channel you're watching, and going full screen from the preview doesn't reload it.
- **Program reminders**: in the guide, move ahead in time and press OK on a program that hasn't started. A card shows up when it begins, on any screen.
- **Speed test** (Settings › Advanced › Diagnostics): measures your addons and stream servers and tells you the largest file that plays without stopping, per 2 h movie and per episode.
- **Change artwork** (#142): a new button on the title page lets you pick the backdrop and the logo from TMDB, fanart.tv and the other sources. Your pick is used on the home hero, the title page and the player, and is saved per profile.
- **Saved panel**: hold OK on a title for More info, Remove or Mark as watched.

## Changed

- **Settings** follows the web app's layout: Account, Appearance, Layout, Content, Integrations, Playback, Trakt and Simkl, Advanced and About, with collapsible groups. Toggles flip with a single OK. Signing out and removing an IPTV portal ask for a second OK.
- **Sidebar pinned open**: Library, Search, Diagnostics and Friends no longer draw under it.
- **Smoother gradients**: dark gradients no longer show hard bands on OLED TVs.

## Fixed

- **Live channels**: a source that takes a while to open (like 4K) is no longer skipped for ones that never answer.
- **Anime (ASS) subtitles** (#92): the header, fonts and first minutes are read before the video starts, the app waits longer when the server refuses, missing fonts no longer hand the subtitles to the TV, and signs stay inside the picture on 4:3 and widescreen video.
- **Collection GIFs** (#141): a folder whose cover is a GIF now animates even without a focus GIF, a GIF removed from the disk cache is fetched again, and the log says why a card doesn't animate.

## Notes

- Samsung: the package is unsigned on purpose; sign it with your own certificate as before.
- LG with the Homebrew Channel: add `https://github.com/iqui27/nuvio-native-legacy/releases/latest/download/repo.json` as a repository to get updates from the channel.
- The speed test downloads a few seconds of real streams. With some debrid services that can make the file show up in your debrid panel.

## Packages
- `space.nuvio.native.legacy_1.5.0_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.5.0_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.5.0-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
- `repo.json`, `webosbrew.manifest.json` — Homebrew Channel repository files.
