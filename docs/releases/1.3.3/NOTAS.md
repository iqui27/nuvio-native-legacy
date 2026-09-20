The backdrop you pick in Xperience becomes the collection's hero, collection page tabs match the rest of the app, Trakt comment cards are smaller, and on Samsung the home no longer stutters while a row's artwork loads (#72). LG normal builds get the "Update now" button back.

## Added

### Xperience backdrops
![Netflix with the Spotlight backdrop](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.3/hero-spotlight.jpg)

Xperience now offers 17 backdrop styles per brand (Carbon Mono, Spotlight, Radial Bloom, Two-Hue, Monogram…), 3840×2160, tuned to each brand's colors. Pick one in Xperience ("Apply backdrop set", per folder or for a whole collection) and the app uses it as that folder's hero on the home and on the collection page, full-bleed with the brand logo on top. A chosen style wins over the art shipped in the package; the default (plain gradient) keeps the packaged art, so nothing changes until you pick.

![Disney with Radial Bloom](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.3/hero-radial-bloom.jpg)

The 4K WebP is decoded already reduced to the hero size — inside libwebp on LG (`use_scaling`), in the browser on Samsung — so a 33 MB full-size bitmap never exists.

## Fixed

- **Samsung: home stutters while a row's artwork loads, sometimes crashes** (#72, AU7000). 1.3.2 moved image decoding to the browser, but the resize and the pixel readback still ran on the main thread between frames — up to 1 s per image on that TV (`swap=1004` in the frame log, 2 fps while a row loaded). Decoding now runs in its own Worker (`decodificador.js`) with an OffscreenCanvas and writes pixels straight into shared memory; the main thread only forwards the request. Falls back to the old path if the Worker does not start. Verified in a real Chromium on both paths; not yet on a Samsung — please report.
- **LG normal build had no "Update now" button since 1.3.1.** The 1.3.1 and 1.3.2 assets were named `NuvioTV-x-webos.ipk`, which does not end in `_arm.ipk` as the updater expects. This release goes back to the contract names, and the app accepts both from now on.

## Design

- **Collection page tabs** now use the same grammar as the Saved panel: width by label, 52 px, focus filled in the accent color with dark text, no outline; the open tab is the lighter surface. The strip scrolls by cursor instead of six at a time.
  ![Collection tabs](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.3/collection-tabs.jpg)
- **Trakt comment cards** on the detail page shrunk from 722×466 to 600×340, six lines of text, footer in the same place.
  ![Trakt comment cards](https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/docs/releases/1.3.3/trakt-comments.jpg)

## Notes

- What's new card shown once.
- Samsung confirmation for #72 is pending; the Worker path logs `[webp] navegador decodificou o primeiro: … no worker`.
- `docs/hisense-vidaa.md` (Portuguese): what a Hisense VIDAA port would take.

## Packages
- `space.nuvio.native.legacy_1.3.3_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.3_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.3.3-tizen.wgt` — Samsung Tizen (WASM).
