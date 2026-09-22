Episodes: the player stops picking up the previous episode's sources, the carousel opens on the episode you were on, and finishing an episode marks it watched (#100, #101, #102). TorBox and Premiumize accounts now work like Real-Debrid. The last poster in a row is no longer clipped (#103), and rows start at the first poster when you reopen the app (#95).

## Playback

- **The player sometimes loaded the sources of the episode you had just finished** (#101). The source list was global and anonymous — nothing recorded which episode it had been fetched for — and it was only thrown away when it was older than 60 s. Now every list carries the title and the episode it belongs to, and anything else is discarded before the player opens.
- **The episode carousel jumped back to the first episode of the season** (#102). It tracked the row number, and the catalog clears the episode list for a few frames every time the add-ons publish again — so the focus fell back to episode 1, including while you held OK to open the options. It now tracks the episode number and finds it again when the list returns; it also opens already scrolled to the episode instead of sliding there.
- **Episodes were not marked as watched** (#100). Two different thresholds decided the same thing: the next-episode card declared the end at the credits marker (or the last 2 min), while the player only rounded up to the end in the last 60 s. In an 18 min episode the card appears at 88.9 %, and accepting the next episode the app itself offered marked nothing. One threshold now, and the check mark is written together with the progress bar.
- **"Up next" appeared before the credits** (#73). On LG the Matroska probe — the same one that brings the chapter marker for the credits — was only armed when a subtitle track was missing its language, so any MKV without subtitles had no marker at all and fell back to the "last 2 minutes" rule. Samsung always probed. Fixed.

## Sources

- **TorBox and Premiumize** are resolved like Real-Debrid, using each service's own cache check — only what is already cached plays. In the logs of the 1.3.12, 13 of 26 people had one of those services configured and saw their torrent sources silently discarded (one session dropped 145).
- **Add-ons that were never queried** (#83). Add-ons read from the local list were created switched off, so they were skipped for sources, subtitles and catalogs without printing a single line. Fixed, and the log now names who was left out of a source search and why ("switched off", "the manifest declares no stream", "no manifest yet").

## Home and screens

- **The last poster in a row was cut off at the right edge** (#103). The scroll target measured the poster at rest; with the focus ring on, the margin worked out to zero. It now measures the poster as drawn in focus, including the 16:9 expansion and the ring.
- **Rows remembered the column after a restart** (#95). The position is no longer written to disk: reopening the app starts every row at the first poster. Going into a title and back still returns you where you were.
- **New look** for Library (bigger rows with year, runtime, genres and the IMDb badge; compact filter pills), Schedule (narrower cards, three lines of detail, a new reminder bell), Saved, the context menu and the What's new card. One table of buttons and badges now serves every screen, so sizes and colors match.

## Notes

- Samsung: the package is unsigned on purpose — a distributor certificate locks the install to a fixed list of TVs. Sign it with your own certificate as before.
- The TorBox route that creates the torrent uses `multipart/form-data` because that is what the official SDK sends; if a TorBox source fails, please send a log.

## Packages
- `space.nuvio.native.legacy_1.4.0_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.0_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.0-tizen.wgt` — Samsung Tizen (WASM).
