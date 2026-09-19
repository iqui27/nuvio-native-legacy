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

## Everything in this release, item by item

**Notice center**
- Toast in the corner, one line, with the count and the key that opens it; the timer only starts once the home is on screen (not behind the profile chooser).
- Notices list: friend recommendation, premiere with reminder, update, maintainer note, crash. New = accent dot next to the icon. OK acts per type; Back closes and marks everything read; read state persists on disk per TV.
- BLUE (LG) / CH+ (Samsung) open the list while the toast is visible; outside the toast both keys keep their old jobs (Saved panel, guide section jump).
- **NOTICES** tab in the Saved panel, always present; the SOCIAL tab only when the recommendations service is built in. Count badge on the tab.
- Maintainer channel: `avisos.json` in the repository, read every 30 minutes, with `desde`/`ate` window, `plataforma` (todas/lg/tizen), `ate_versao`, and English variants of title and text.
- Crash detection: session marker written at start, removed on clean exit; on the next start, a card offers **Send log** / **Not now**, once per crash. The previous session's log is preserved before the new one truncates it (webOS).
- Log upload: last 200 KB, credentials already stripped, POST to the recommendations service; the server caps at 200 KB and keeps it 30 days.

**TV Guide**
- Channel list cached on disk per profile and shown immediately on open; the network refreshes it behind and only republishes when the list actually changed (order-independent signature), so focus and scroll don't reset.
- Manifests are no longer fetched by the guide: it reuses the channel catalogs the home already parsed at startup, in parallel. A manifest is fetched only if that add-on's hasn't arrived yet.
- Network returning nothing (all add-ons down) keeps the list you had instead of wiping it.
- Subtitle says "N channels · M categories · updating…" while the refresh runs.
- Channel add-ons panel: Spotlight is a tag next to the name (it sat on top of the description); every line's text stops before the pill; suggestions get two-line descriptions.

**Artwork**
- Backup from TMDB when `images.metahub.space` fails: posters (w342), backdrops (w1280) and episode stills (`/tv/{id}/season/{s}/episode/{e}`), found by IMDb id, cached under the original URL. Logos have no backup.
- Samsung: JPEG, PNG and WebP are decoded by the browser and shrunk on a canvas to the size that will be shown; only that crosses into the WASM heap. GIF keeps its own path.
- Memory for images in Settings (Automatic/96/160/240/300/400/512 MB), applied live, capped by RAM (< 1.2 GB: 96; < 2 GB: 160; < 3 GB: 300; ≥ 3 GB: 512; Samsung: automatic). Per TV, never synced. The images panel shows "chosen in Settings" as the ceiling's source.

**Playback sources**
- MP4 gets priority within the same resolution on LG (never over a higher resolution); Dolby Vision in MP4 still ranks above everything. MP4 pill in the sources sheet. Not applied on Samsung.

**Detail page**
- Mark as watched: one OK (#70), with a confirmation (check + "N episodes marked/unmarked", or "nothing to change") that closes by itself.
- Action buttons 94/96 → 72 px, padding 54 → 38, icon 28 → 22, gap 24 → 18; focus still by scale.
- Drop-off radar: full 0–100 axis when the drop is ≥ 20 pp; the zoomed axis stays for small drops.

**Home**
- Down from the hero lands on the first row; the restored position keeps each row's column and scroll.
- With the hero focused, rows sit lower: header of the first row visible and 15% of its cards.

**Under the hood**
- Recommendations service: `/v1/registro` route and `registro` table (30-day retention). `tools/arm.sh` accepts `NUVIO_SSH_OPTS`; `tools/build-local.sh` fixed (ran from the wrong directory). Tests: `tests/artereserva.sh`, `tests/webp-tizen.sh` (JPEG cases), `tests/episodios_shot.sh` (single-OK), `tests/homepos.sh` updated.

## Notes

- Guide, Samsung decode and artwork backup were verified on the Mac preview (the backup with a proxy blocking only metahub) and the decode path in a real Chromium; Samsung hardware confirmation is still pending.
- The notice center's log upload goes to the recommendations service. It requires the account or Trakt login the app already has, keeps logs for 30 days, and stores no token or IP.
- Maintainer notices come from `avisos.json` on the `master` branch; the format is documented in `AVISOS.md`.

## Packages
- `NuvioTV-1.3.2-webos.ipk` — LG webOS (arm). Default texture budget.
- `NuvioTV-1.3.2-webos-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.2-tizen.wgt` — Samsung Tizen (WASM).
