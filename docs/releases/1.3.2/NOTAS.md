The TV Guide opens at once, artwork has a backup when its server is down, Samsung decodes images outside the WASM heap, MP4 comes first on LG, and the app can now tell you things: a notice center with friend recommendations, premieres, updates, notes from the maintainer, and a "send the log" card when it crashed.

> Everything in the TV screenshots below is the real app. Charts are arithmetic or request counts, not stopwatch measurements; where something was measured, the notes say where.

## Added

### Notice center
![Toast on the home screen](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/home-toast.jpg)

A one-line toast in the corner when something arrives (**BLUE** on LG, **CH+** on Samsung opens it). The list lives in the Saved panel as a third tab, **NOTICES**, so you can open it whenever you want; new items carry a dot. Five sources feed it:

- a friend's recommendation → OK opens Saved
- an episode of a show you set a reminder for → OK opens the title
- a new version on GitHub → OK opens the update card
- a note from the maintainer, from `avisos.json` in this repository (read every 30 min): known issues while they are not fixed yet, with a validity window and an optional "up to version"
- the app closed on its own (see below)

![NOTICES tab in the Saved panel](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/notices-tab.jpg)

### "The app closed on its own" — send the log
![Crash card](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/crash-card.jpg)

The app writes a marker at startup and deletes it on a clean exit. If the marker is still there on the next start, the previous session died (crash, killed by the TV for memory, power cut) and you get this card once. **Send log** posts the last 200 KB of that session's log — already stripped of passwords and keys — to the recommendations service; **Not now** closes it. Nothing is ever sent without the button. On Samsung there is no log file, so only version, platform and date go.

### Memory for images, in Settings
![Memory for images](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/settings-image-memory.jpg)

Automatic / 96 / 160 / 240 / 300 / 400 / 512 MB. It applies immediately and is capped by what the TV's RAM supports (a 2 GB TV stops at 300; 400 and 512 only pass on 3 GB or more; Samsung stays on automatic, where a bigger cache measured slower). Per TV, never synced to the account. The "Memory used by images" line next to it is a read-out, not a setting (#71).

### MP4 first, on LG
![MP4 pill in the sources sheet](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/sources-mp4.jpg)

Among sources of the same resolution, automatic now picks the MP4 — the container that plays Dolby Vision on this TV. A 1080p MP4 still does not beat a 4K MKV. The sheet marks each MP4 with a pill. Not applied on Samsung, where AVPlay reads MKV without that penalty.

### What's new card
![What's new card](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/whats-new-card.jpg)

One page, shown once.

## Performance

### TV Guide opens at once
![Requests before the guide list appears](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/guia-pedidos.png)

The guide used to exist only after a full pass over the network on every start: one `manifest.json` GET per add-on in your account, in series, 15 s timeout each (most add-ons have no channels at all), then the channel catalogs page by page. Two changes: the manifests are the ones the home already read at startup, in parallel, so the guide no longer fetches them; and the last channel list is kept on disk (per profile) and shown immediately, while the network refreshes it behind and only swaps the list if it actually changed. If every add-on is down, the list you had stays.

### Samsung: images decoded by the browser
![Bytes into the WASM heap per image](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/tizen-heap.png)

On Tizen every JPEG and PNG went through a software decoder at full size inside the fixed 256 MiB WASM heap — the same heap the player needs. Logs from #69 (AU7000, Tizen 6.0) show a 3840×2160 backdrop taking 13 s to decode and the heap down to 6.7 MiB free; #68's playback crashes fit that picture. Now the TV's own browser decodes the image and shrinks it to the size that will be shown, and only that crosses into the heap. Verified on a real Chromium with `tests/webp-tizen.sh`; not yet confirmed on a Samsung TV — please report.

### Artwork with a backup
![Home with all artwork served by the backup](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/art-fallback.jpg)

Posters, backdrops and episode stills from Cinemeta, the Trakt watchlist and the account library all come from `images.metahub.space`. When that server fails or answers 404 (it did on 17–19 Sep), the same image is fetched from TMDB by IMDb id and cached under the original address — the rest of the app never knows. The screenshot above was taken with metahub fully blocked: hero and Continue Watching came from the backup. Closes the "Artwork unavailable" rows in #67 and #55.

## Fixed

- **Mark as watched needed two OK presses** (#70). The menu opens on the release of the long press and still swallowed the next OK as if it were that release. One press applies now, and the menu confirms with a check and "N episodes marked" before closing.
  ![Mark as watched confirmation](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/mark-watched.jpg)
- **Pressing down from the home hero jumped several rows down.** Yesterday's position was being restored as the first row below the hero. Down now lands on the first row; each row still remembers its own column.
- **Drop-off radar exaggerated normal drops.** A drop bigger than 20 pp is now drawn on a full 0–100 axis; small drops keep the zoomed axis so a 3-point loss is not a flat line.
  ![Drop-off radar on a full axis](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/dropoff-radar.jpg)
- **Channel add-ons panel**: the Spotlight badge sat on top of the description; it is a tag next to the name now, and suggestions get two lines of description.
  ![Channel add-ons panel](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/guide-addons.jpg)

## Design

- **Detail page buttons** shrunk from 94/96 px to 72 px — the hero's scale — with padding, icon and gaps in proportion. They weighed four times the text next to them.
  ![Detail page action buttons](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/detail-buttons.jpg)
- **Bigger hero on the home**: with the hero focused, only the header and the top 15% of the first row show below it.
  ![Home hero with 15% of the first row](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.2/home-hero.jpg)

## Notes

- Guide, Samsung decode and artwork backup were verified on the Mac preview (the backup with a proxy blocking only metahub) and the decode path in a real Chromium; Samsung hardware confirmation is still pending.
- The notice center's log upload goes to the recommendations service. It requires the account or Trakt login the app already has, keeps logs for 30 days, and stores no token or IP.
- Maintainer notices come from `avisos.json` on the `master` branch; the format is documented in `AVISOS.md`.

## Packages
- `NuvioTV-1.3.2-webos.ipk` — LG webOS (arm). Default texture budget.
- `NuvioTV-1.3.2-webos-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.2-tizen.wgt` — Samsung Tizen (WASM).
