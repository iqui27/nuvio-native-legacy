Samsung: no more out-of-memory crash from 4K backdrops, and a lighter start. Player: the controls come back with focus on Play (#109), and an audio track the TV can't decode is now a notice instead of a dead source. The Xtream username keyboard shows the digits (#88). When no source is found, the sheet now says why.

## Samsung

- **Out-of-memory crash** — the only fatal crash in the 1.4.0 logs. With image quality on High, backdrops were requested in TMDB's `original` size (3840×2160). On Samsung the image is decoded by the browser at full size — 33 MB each — inside a 256 MB heap: one log shows three of them in 3 s, free heap down to 5.7 MB, then the abort. Backdrops and episode stills now stay at 1280 px on Samsung. LG keeps `original`, because there the JPEG is scaled down while decoding.
- **Lighter start.** The built-in fallback artwork went from 1600×900 to 1280×720 (42 % smaller); one log had a 1.4 s freeze decoding the first of them at launch.
- **Play/Pause on the remote** now pauses. It used to arrive as OK, so with the focus on Subtitles it opened the subtitle sheet instead.

## Player

- **Focus lands on Play** when OK brings the controls back (#109). Before, it stayed wherever it was last time (Subtitles, Audio, Aspect), and the next OK opened that sheet.
- **"No source" error cleared by itself.** After a search that found nothing, the error could vanish and leave a black screen: the app mistook the title page's trailer, which uses the same video pipeline, for a source that had started playing. Only a video opened by the player counts now.
- **LG: audio the TV can't decode** (for example EAC3 5.1 on some webOS 5 models). The TV reports it and keeps playing the picture with no sound; the app treated that as a dead source. It is now a notice — "This TV can't play this source's audio. Switch the source or the audio track."

## Sources

- **An empty source sheet says why:** none of your add-ons has this title, a given add-on didn't answer, no stream add-on is installed, they are switched off, or torrents were dropped for lack of a debrid service.
- **Debrid errors are logged with the service's own reason** (key masked). An account-level refusal (401/403/429, plan or active limit) takes that service out of the rest of the search instead of being retried for every torrent. This is to explain the TorBox `403` seen in one log; its cause is not known yet.

## Other

- **Xtream username keyboard** (#88): all 73 characters now fit, digits included — it used to cut off after "P".
- When the app did not close cleanly, the next start logs what the previous session last did (went to background, was visible, memory), so a crash can be told apart from the TV closing the app.

## Packages
- `space.nuvio.native.legacy_1.4.1_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.1_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.1-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
