# Nuvio Legacy 2.0.8

Changes integrated after snapshot tag `v2.0.7-test.2` (existing release history retained). Updated from local commit and branch snapshots; device validation remains pending.
Community: **https://discord.gg/9NWr6SHyzJ**

Source for the in-app What's New card and release notes. English follows the owner's current language override.

## Highlights

- **More precise Samsung playback controls**: Seek more accurately with the reworked seek gesture, inspect local network logs, and avoid source ghosting. Platforms: Samsung .tpk. Refs PR #640, @Namer03, `f67359b37`, `49714272a`, merge `178d3da67`.

## New

- **Optional webOS Dolby Vision conversion**: Enable optional Dolby Vision P7 to P8.1 conversion while bounded DTS recovery remains independent of that conversion choice. Platforms: webOS. Refs `a790e161c`, `9c5062588`, `1998c125f`, `845fe8cb1`, `087d3a015`, `d5e0185cc`.

- **Automatic performance defaults**: Start performance settings in Automatic mode and migrate saved choices to the updated selection model. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `fcdae1879`.

- **Android tunneled playback**: Offer an optional hardware audio/video synchronization setting, disabled by default. Platforms: Android. Refs #626, owner request, `19e638ea1`; `agente/205-205-tunnel`.

- **Android cache controls**: Clear regenerable cache while preserving durable user data, APK staging and viable torrent streaming windows; superseded update APKs are removed. Platforms: Android. Refs owner request; d8699b55c, e731a7847, d35f60069, 70b1f55e3; merges a1169f5ad, fcc581387.

## Fixes

- **Stable library membership**: Preserve complete library synchronization snapshots and removal evidence, and keep local saved titles while their add action is pending. Platforms: Samsung .tpk; shared library code on webOS, Android and WASM. Refs #639, `75a1c56ae`, `515408fe5`, merge `afcf66f36`.

- **Safer local network logs**: Remove URL credentials, nonstandard URL schemes and long tokens from local network logs. Platforms: Samsung .tpk. Refs PR #640, `b05a33bcc`, merge `82f8b16a9`.

- **Empty add-on responses stay valid**: Accept an empty streams array as a valid response when an add-on has no sources to offer. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `d4d850204`.

- **Large Live TV requests complete**: Handle long stream requests without the obsolete channel limit. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `a5f0675e5`, merge `b02444677`.

- **Stage backgrounds appear immediately**: Resolve stage backdrops from Home without first opening the title details. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `4befffb89`, merge `5eb14e909`.

- **Plugin-only source errors**: Recognize enabled local scrapers when explaining unavailable sources, so plugin-only users do not receive a false missing-add-on message. Platforms: webOS, Android, Samsung .tpk, WASM with local plugins. Refs owner report, `83fe3c59a`, merge `7a13a6bb3`; `agente/205-205-semaddon`.

- **More usable playback memory**: Lower the buffer safety floor and show the RAM available for playback. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `84d89ab0d`.

- **GIF avatars keep their proportions**: Crop animated avatars centrally without stretching their original aspect ratio. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `58ccca756`.

- **Subtitle provider and synchronization labels**: Show provider names and synchronization labels while excluding informational tracks from playback choices. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `9f9ac0eb8`, `30142a7e6`.

- **Long anime episode catalogs**: Preserve episode catalogs when large source file lists exceed capacity. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `bfcd991a2`.

- **Episode menus fit your screen**: Keep episode actions and ratings inside the screen when interface scaling is enabled. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `3d19bb70d`.

- **Readable Arabic bold subtitles**: Honor bold text even when the Arabic fallback font supplies only a Regular face. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `7c8f1ea73`.

- **Samsung ASS playback**: Avoid handing ASS subtitle tracks to the TV decoder, preventing playback stalls. Platforms: Samsung .tpk. Refs #638, PR #638, `5fb8810aa`.

- **Top ranking controls**: Preserve root catalog identities while parsing manifests so configured TOP X catalogs remain available. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `c95fa330a`; `agente/205-205-topx`.

- **Plugin source labels**: Keep source names anchored and retain titles alongside descriptions so sources remain identifiable. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `884609d90`; `agente/205-205-plugmeta`.

- **Live TV automatic quality**: Preserve channel resolution on full accounts and correct automatic HLS quality selection and unreliable speed advice. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `ecad7d1e2, 76cd556a1`; `agente/205-205-livetv720`.

- **Distinct AIOStreams instances**: Keep separately configured AIOStreams instances distinct by their transport URLs. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `82ee52a2b`; `agente/205-205-aiomulti`.

- **Posters follow metadata language changes**: Refresh cached catalog posters when the metadata language changes so browsing artwork follows the new selection. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `21dc9f41f`, merge `564b557e2`.
- **Clearer frame-rate overlay**: Label the performance overlay as UI fps so its interface frame rate is distinguishable from video playback cadence. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `15af443d8`, merge `84eae41cf`.
- **Custom posters across catalogs**: Apply configured poster services consistently across browsing surfaces and support shape-dependent URL templates. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `a25def053`, merge `c450d139d`; authenticated service rendering still needs device validation.
- **Expanded-card trailers fill their frame**: Keep focused-card trailer placement in its own coordinate space so video fills expanded artwork at different text scales. Platforms: webOS, Android, Samsung .tpk. Refs owner request, `8271a8dbb`, merge `a4d52842e`.

- **Interface follows your language again**: Restore sorted translation keys so selecting another language no longer leaves the interface in Portuguese. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner incident; `98ed41d91`.
- **720p interface keeps rendering on weaker GPUs**: Start app-side 720p upscaling on 4K panels and retain the 720p surface when GPU upscaling is unsupported. Platforms: Android. Refs `8c083d04c`, `e85d5a89d`; merge `1890f9f67`.
- **Resume survives TV power loss**: Save durable playback checkpoints while keeping tracker playback active during checkpoint updates. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request; 5623ab442, ceca13b7e; merges 9e6c12d47, eb5203637.
- **LG buffer presets apply to direct video**: Apply the chosen buffer preset to direct HTTP video on demand. Platforms: webOS. Refs owner request; 6c9bc485b; merge 6c8233ded.
- **Live diagnostics on limited accounts**: Run explicitly requested live-TV diagnostics even when the account has limited access. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request; 6a392a97f; merge bf6df9a8e.
- **Ranked posters keep their shape**: Keep numbered and stacked ranking rows in portrait format, including artwork loaded ahead of scrolling. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request; b19d8ec28; merge 35caf5df8.
- **Simkl anime in Continue Watching**: Preserve absolute episode numbers and fallback IDs, using provider metadata to map long-running anime to the next episode. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner report, `1d2ab0a5b`, `af9250ab5`, merge `5d30131d6`. Cached anime bodies now also accept numeric next-episode values. Refs `5bc056384`, merge `0f18626ad`.
- **Complete Simkl library lists**: Keep library entries across all Simkl statuses so watched and watching lists do not lose titles during synchronization. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner report, `30efe6903`, merge `3e709ec43`.
- **Stable Samsung live picture size**: Reset geometry when switching channels and settle guide transitions once, preventing crops from the previous channel and visible zoom steps. Platforms: Samsung .tpk. Refs owner report, `c3ac7d1ae`, merge `e3cd59e71`.
- **Samsung playback overlays stay visible**: Keep API11 window rendering active over static video holes so playback controls remain drawn. Platforms: Samsung .tpk. Refs owner report, `b3c4a5e36`, merge `f3f5af5ff`. Device smoothness remains unverified.
- **Live playback avoids competing probes**: Skip unnecessary media probes while live channels play, reducing interference with channel playback. Platforms: Samsung .tpk. Refs tester report, `8847255a3`, merge `b206981ce`. Continuous HLS playback still needs device confirmation.
- **Torrent sources start without waiting for optional cues**: Limit speculative tail reads to one second during local torrent startup and continue without optional Matroska cues when those pieces are unavailable. Platforms: webOS, Android, Samsung .tpk; native DTS/P2P path. Refs tester report, `d4cb60161`, merge `0a1a32a13`.
- **Samsung opens at full size**: Ignore the temporary 1×1 startup surface to prevent the black screen that previously required reopening the app. Platforms: Samsung .tpk (Tizen 9/10). Refs #624, #628, #629, `f840b4037`. TV validation pending.
- **No false interrupted-launch notice**: Closing the app normally through the TV no longer triggers a “Previous launch stopped” notice on the next launch. Platforms: Samsung .tpk. Background eviction after normal playback is also treated as a normal close. Refs `36a2d1f13`, `6755ff560`, merge `ad50fc8f5` (no fix issue supplied).
- **Torrent playback with less free memory**: Allow a 160 MB streaming window so TVs with roughly 460 MB free memory can start torrents previously rejected for insufficient space. Platforms: webOS. Refs `8e67f7be0` (no issue supplied).

- **Live channels start on LG TVs**: Let playback discover its default tracks before selecting video, preventing premature track selection from blocking TS and HLS startup. Platforms: webOS. Refs #625, `abe573043`, merge `dda4428e4`.
- **Responsive Samsung episode endings**: Keep playback controls responsive during stalls and record episode completion at the end without restarting recovery after playback finishes. Platforms: Samsung .tpk. Refs owner request, `06a178e07`, merge `b22929845`.
- **Home loads with slow DNS**: Bound DNS retries and reuse successful answers across requests so slow resolution does not repeatedly exhaust connection deadlines. Platforms: Samsung .tpk. Refs #630, `25eac3a93`, merge `beb30e5b9`.
- **Older LG TVs stay open after profile selection**: Use text surfaces compatible with SDL 2.0.4 to prevent the missing-symbol crash after choosing a profile. Platforms: webOS (older SDL runtime). Refs owner request, `7dc80ce7c`, merge `921210932`.

## Performance

- **Android artwork stays loaded**: Ignore background memory trim signals while the activity is visible so posters do not repeatedly disappear and reload. Platforms: Android. Refs `1aedd966b`.

- **Player frame-rate handling**: Bound browser frame waits while player menus and overlays are drawn to keep controls responsive. Platforms: Samsung .tpk, WASM. Refs owner request, `2206b5a27`, `10096c8ca`; `agente/205-205-playerfps`.

- **Artwork loading retries**: Defer artwork uploads when visible images fill memory and skip blocked uploads so smaller posters can still load. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `55d077c80, b7d7ed0e8`; `agente/205-205-imgloop`.

- **Useful scrapers run first**: Remember recent scraper hits per profile, defer anime-only scrapers for other titles, and show checked, queued and cut counts when no sources are found. Platforms: webOS, Android, Samsung .tpk, WASM with local plugins. Refs owner request, `cfe0215b4`, merge `f215e7fd8`.

- **Manual graphics choices survive automatic recovery**: Order automatic frost and animation throttling and keep manual effects selections when adaptive reductions reset. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `4c7b5e47a`, `0b7036d03`, `c271714aa`; merge `59633662a`.

Low-memory playback and bounded torrent-startup reads are listed under Fixes.

## Community

- **Samsung playback and network tools contribution**: Enjoy precise seeking, revised seek gestures, network logging, and the source ghosting fix contributed by @Namer03. Platforms: Samsung .tpk. Refs PR #640, `f67359b37`.

- **Samsung subtitle contribution**: Playback benefits from the ASS decoder fix contributed by @Namer03 in PR #638. Platforms: Samsung .tpk. Refs PR #638, #638, `5fb8810aa`.



## Internal

- **HTTP and DNS regression fixtures**: Keep strict DNS and webOS HTTP buffer regression fixtures buildable. Platforms: webOS; host verification. Refs `5fd18864f`, `83072bc99`.

- **Subtitle log translation scan**: Classify the native playback reason as log-only during translation scanning. Platforms: Samsung .tpk. Refs `433b0f00a`, #638.

- **Translation regression coverage**: Restore language-test coverage and verify translation-key ordering to catch language-selection regressions. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `3b3906186`, merge `d4f8c2845`.
- **C9 artwork comparison**: Verify native C9 artwork against the earlier rendering baseline while preserving both capture modes. Platforms: webOS; host verification. Refs `bdc7d7c8c`, merge `d072c891b`.

- **Browser build link fix**: Restore the browser build's missing network-function stub so Samsung browser packages can link successfully. Platforms: Samsung browser/WASM. Refs `b84091f4c`.
- **DV7 converter builds on the host**: Build the host-side Dolby Vision converter outside the container so the ARM build has the required Rust toolchain. Platforms: webOS build. Refs `96c2ba90e`.

- **Issue tracking**: Keep release and issue tracking current. Snapshot refs: `d023e32ef`, `0d33a122b`, `0870b8caa`, `58342b5ce`. Additional refs: `d1563d5b3`. Snapshot refs: `cebe243ac`, `88f1a6d8a`, `1a9cf77eb`, `1e9121e7f`. Additional refs: `80a3b437b`. Additional refs: `612a2ecd2`. Additional refs: `c9e4f8038`. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `609ed3cc3`, `ad6ec6fb9`, `89b48afe7`, `12bedced8`, `d2a392e14`, `dacee39da`, `9460c0dd8`, `e901b95e7`, `f1743efe5`, `009bba7ae`, `324182411`, `a580c7eae`.

## Pending (not integrated yet)


- **Add-on subtitle synchronization**: Subtitle add-on synchronization work awaits review before its final user-facing behavior can be confirmed. Platforms: Confirm from review. Refs agente/205-205-legaddonsync.
Earlier report named `agente/205-i429hbs` (#429), `agente/205-p2pusb` (#334, #505), `agente/205-i513autoplay` (#513), `agente/205-barra27`, PRs #489/#490/#491/#493/#498, `agente/205-dicatoque` (#472), m3umisto, palco2, logos, fileiraui, diretorpol and triagem-velhas as pending. Their branches are absent from the current open snapshot; retain this historical record without presenting them as pending 2.0.8 features or duplicating already released work.

- **Torrent engine follow-up**: Further torrent-engine work remains outside integration and needs review of the newer fix log before its user-visible scope can be confirmed. Platforms: confirm from review before release. Refs `agente/p2p`; current `open.txt` and review snapshot.

Other open branches are historical work or deferred plans; see the tracking map and external `estado.md`. No new feature or external contributor credit is inferred from branch names.
