# Nuvio Legacy 2.0.8

Playback, memory and catalog fixes after 2.0.7, plus work integrated from community contributions. Community server: **https://discord.gg/9NWr6SHyzJ**

## Highlights

- **Samsung opens at the right size.** The temporary 1×1 startup surface is ignored, addressing blank-screen reports on Tizen 9 and 10 (#624, #628, #629). Confirmation on the affected TVs is still pending.
- **More precise Samsung controls** (PR #640, @Namer03): revised seeking and seek gestures, local network logging and a source ghosting fix. Follow-up work removes credentials and long tokens from those logs.
- **Samsung ASS playback no longer uses the TV subtitle decoder** (PR #638, @Namer03), avoiding the decoder stalls on those tracks.
- **Optional Dolby Vision P7 to P8.1 conversion on webOS**, contributed by @Matis94. It is off by default; bounded DTS timestamp and HTTP range recovery remain independent of the conversion setting.
- **Player format row** (PR #632, @I0-oX): choose Pill, Text, Marks, Badges, Original or Off. Original remains the default. The optional row shows resolution, HDR, audio format, language and channels.
- **Android tunneled playback** (#626): an optional hardware synchronization setting, off by default.
- **Resume survives TV power loss.** Durable playback checkpoints are saved without stopping tracker playback during checkpoint updates.
- **Library and Simkl fixes.** Complete library snapshots and pending saves are preserved (#639); Simkl keeps all list statuses and maps absolute anime episode numbers using provider metadata.
- **Less artwork churn.** Visible Android screens ignore background trim signals, blocked uploads no longer prevent smaller posters from loading, and webOS texture pressure handling is more stable.
- **webOS/.tpk texture memory pressure stops flapping.** Sampled MemAvailable pressure holds its band until memory actually recovers instead of expiring and re-arming every 30 s, and the cap can never drop below the artwork already on screen. Slow and repeatedly reloading images on 1-1.5 GB TVs after 2.0.7 are addressed while the #415 low-memory protection stays in place.
- **Android Lite APKs.** Optional per-ABI Lite builds ship without the embedded P2P/torrent engine, and in-app updates match the installed variant so a Lite user is never handed the full APK.
- **Search artwork resolves.** Search results resolve their own backdrops, and title logos fall back to TMDB when metahub has none.
- **Automatic-mode settings rows** turn on their prerequisites, including the Glass Automatic choice, instead of silently doing nothing.
- **The effects ladder ignores transient slowdowns** and advances one rung at a time; subtitle references are downloaded once, without range errors.
- **Automatic performance controls** reject short GPU slowdowns, reduce one step at a time and require measured gains before retaining a reduction. Manual effect choices survive recovery.
- **Live TV fixes** cover LG startup (#625), Samsung picture geometry, Automatic HLS quality, long channel requests and diagnostics on limited accounts.
- **Translations work again.** Sorted translation keys restore language selection; Arabic subtitle bold also works with Regular-only fallback fonts (#637).

## New

- Optional player format row and its Settings preview (PR #632). Pill, Text, Marks and Badges offer alternative presentations; Original preserves the previous corner marks, and Off hides them. The choice is local to this TV, with six new strings in all 29 translation tables.
- Optional webOS Dolby Vision P7 to P8.1 conversion. The conversion path has its own setting and build support; DTS recovery does not depend on enabling it.
- Android tunneled playback setting (#626), disabled by default, for devices that support hardware audio/video synchronization.
- Android **Clear cache** action for regenerable storage, while keeping durable user data and staged updates.
- Local network logs and revised seek gestures from PR #640 on Samsung .tpk.
- Playback memory overlay shows RAM available for buffering; the buffer safety floor is lower.
- Automatic performance defaults and migration of saved choices to the updated selection model.

## Improved

- Playback checkpoints persist through power loss; periodic checkpoint writes keep Trakt and Simkl playback active.
- Complete Simkl library lists retain titles across watched, watching and other statuses.
- Simkl Continue Watching retains absolute anime episode numbers and fallback IDs, maps long-running anime through provider metadata, and accepts numeric next-episode values in cached responses.
- Library synchronization preserves complete snapshots and evidence of removals; local saved titles remain visible while their add action is pending (#639).
- Separate AIOStreams configurations keep their identity by transport URL instead of collapsing into one instance.
- Local plugin scrapers with recent hits run first per profile; anime-only scrapers are deferred for other titles. Empty rounds report checked, queued and cut counts.
- Source labels keep their anchored names, titles and descriptions, making local plugin results easier to identify.
- Empty `streams` arrays are valid add-on responses when no source is available.
- Long Live TV stream requests complete without the obsolete channel cap.
- Explicit Live TV diagnostics run even on accounts with limited access.
- Catalog manifests preserve root catalog identities, keeping configured TOP X entries available.
- Custom poster services apply across browsing surfaces and support shape-dependent URL templates.
- Posters refresh after metadata language changes; ranked and stacked rows retain portrait artwork, including images loaded before scrolling.
- Stage backdrops resolve directly from Home, without first opening the title page.
- Search resolves backdrops and title logos fall back to TMDB when necessary.
- Expanded-card trailers use their own coordinate space so video fills the frame at different interface scales.
- Subtitle menus retain provider and synchronization labels and exclude informational entries from playable tracks.
- Subtitles are cached for the session, and reference cancellation retains its original reason.
- Episode actions and ratings stay inside the scaled screen; large anime file lists no longer discard the episode catalog.
- GIF avatars preserve their proportions with a centered cover crop (#621).

### Performance and memory

- Android ignores background trim levels while the activity is visible, preventing artwork from repeatedly disappearing and reloading.
- Texture uploads defer when visible artwork fills the memory budget. A blocked large upload no longer blocks smaller artwork behind it.
- webOS texture memory pressure handling stabilizes cache admission and eviction, continuing the low-memory work for #415. The 150/300 MiB thresholds now scale with total RAM, each band holds until available memory clears its recovery margin, and pressure can no longer force the current screen above its own cap. Sampled pressure does not expire on the trim timer, so a TV reporting low memory every 3 s keeps a stable cap instead of re-arming pressure every 30 s.
- Automatic reductions follow the glass, frost and animation order, reject transient GPU slowdowns, advance one rung at a time, and retain reductions only when measurements show a gain.
- Resetting adaptive reductions preserves deliberately selected manual effects.
- Player UI shares frame pacing and retains slow-frame measurements; the overlay labels the rate **UI fps** to distinguish it from video cadence.
- Browser frame waits are bounded while player controls and menus are drawn.
- Optional torrent end-of-file cue reads are limited to one second at startup; playback continues without those cues when the pieces are unavailable.
- A 160 MB torrent streaming window lets TVs with roughly 460 MB free start sources that were previously rejected.
- Android bounds regenerable storage, preserves viable P2P windows and removes superseded update APKs.
- Android Lite builds are optional and split per ABI: `Nuvio-<version>-android-lite-armv7.apk` (~47.8 MB) for Fire Stick class devices and `Nuvio-<version>-android-lite-arm64.apk` (~48.4 MB), against ~68 MB for the full APK. Neither Lite artifact contains the P2P/torrent engine, and the release check verifies the expected ABI libraries per APK.

## Fixed

- **Blank Samsung startup** (#624, #628, #629): ignore the temporary 1×1 EGL surface rather than accepting it as the final display size.
- **Samsung ASS stalls** (PR #638): keep ASS tracks away from the TV's subtitle decoder.
- **Samsung source ghosting and seek accuracy** (PR #640): precise seeking and revised seek gesture handling.
- **Samsung episode-ending stalls:** controls remain usable during stalls, completion is recorded at the end, and recovery does not restart after playback has finished.
- **Samsung live picture crops and zoom steps:** reset channel geometry and settle guide transitions once.
- **Samsung playback controls disappearing over static video holes:** keep API11 window rendering active.
- **False “Previous launch stopped” notices:** normal TV closes and normal background eviction after playback no longer look like interrupted launches.
- **Live TV not starting on LG** (#625): allow playback to discover its default tracks before selecting video.
- **Home catalogs timing out on slow DNS** (#630): bound Tizen DNS retries and share successful resolver answers between requests.
- **Older LG profile-selection crashes:** use text surfaces compatible with SDL 2.0.4.
- **Direct HTTP VOD ignoring buffer presets on webOS:** apply the selected preset to that path too.
- **Library titles disappearing or returning** (#639): retain complete sync snapshots, removal evidence and the local mirror while saves are pending.
- **Arabic subtitle bold ignored** (#637): honor bold even when the fallback font provides only a Regular face.
- **Interface remaining in Portuguese after a language change:** restore sorted translation-table keys and language-test coverage.
- **Stretched GIF avatars** (#621): preserve the source aspect ratio with centered cropping.
- **Missing plugin-only source diagnosis:** recognize enabled local scrapers before reporting a missing add-on.
- **Lost subtitle labels:** preserve provider names and synchronization labels without offering informational tracks for playback.
- **Long anime episode catalogs:** preserve the catalog when source file lists exceed capacity.
- **Live TV Automatic choosing the wrong HLS quality:** preserve resolution even when the account is full and avoid advice based on unreliable speed measurements.
- **Competing Samsung probes during live playback:** skip media probes that interfere with the active channel.
- **720p interface on 4K panels:** enable app upscaling where supported and retain the 720p surface when GPU upscaling is unsupported.
- **Local log credential leaks:** remove URL user information, non-`://` schemes and long tokens from LAN log output.
- **Browser package link failure:** provide the missing `rede_baixar_trecho64_final` stub.
- **webOS DV7 build:** build the converter on the host, where the Rust toolchain is available, before the ARM container build.
- **Regression fixtures:** restore strict DNS, HTTP buffer and language-test dependencies; source priority expectations now include all four options.
- **webOS/.tpk images slow or reloading after 2.0.7:** sampled memory pressure is no longer dropped and re-armed on the 30 s trim timer, and the thresholds scale down to the TV's total RAM, so a 1.2 GB TV keeps one steady texture cap. On-screen artwork is never evicted to satisfy pressure, and the #415 protection for TVs with about 460 MB free still applies.
- **Automatic-mode settings rows doing nothing** (`0ad243e3e`): a row whose requirement is a Ligado/Desligado/Automático choice now enables that prerequisite, so Glass Automatic is applied when the row is switched on.
- **Effects ladder losing its mind after the player:** transient slowdowns are ignored, the first report after a scene or player transition is discarded, and a step is only retained after repeated measured gains. Animations no longer disappear permanently after closing the player on a C9.
- **Subtitle references downloaded three times with range errors:** a reference is downloaded once and its cancellation keeps its original reason.

## Platforms

### LG webOS

- Optional Dolby Vision P7 to P8.1 conversion from @Matis94, with dedicated settings, converter and build support. Conversion stays off unless selected.
- Independent, bounded DTS timestamp and HTTP range recovery; later integration separates recovery from P7 conversion.
- Direct HTTP VOD honors buffer presets, and optional torrent tail probes no longer hold startup indefinitely.
- Live TS/HLS playback starts without premature video-track selection (#625).
- SDL 2.0.4 text compatibility fixes crashes on older runtimes; poster-language refresh also addresses the mapped LG webOS 3 report (#634).
- Texture pressure and upload scheduling reduce artwork churn under memory pressure. Native C9 artwork comparison coverage retains both capture modes.

### Samsung Tizen native (.tpk)

- Startup ignores the placeholder surface (#624, #628, #629); affected-device confirmation remains pending.
- PR #640 adds precise seek, revised gestures, local network logging and the source ghosting fix. Follow-up integration hardens log sanitization.
- PR #638 avoids ASS decoder stalls without taking the PR's README and workflow deletions.
- Live channels reset picture geometry, guide transitions settle once, and competing probes are skipped during playback.
- Static video holes keep the window rendering controls; VOD endings keep controls and completion handling responsive.
- Normal closes and background eviction no longer trigger a false interrupted-launch notice.
- Bounded DNS retries and a shared resolver cache address add-on/catalog delays (#630).

### Samsung Tizen browser (.wgt / WASM)

- The browser build supplies its missing network helper stub.
- Frame waits stay bounded while the player UI is visible.
- Shared catalog, source, subtitle, library, language and artwork fixes apply to this build too.
- Standard and Tizen 3 widget metadata both use the three-component version **2.0.8**.

### Android / Google TV

- Optional tunneled playback (#626), off by default; device support still determines whether the hardware path is usable.
- Regenerable cache can be cleared without removing durable data or APK staging. Superseded update APKs are pruned.
- Visible screens ignore background trim signals, avoiding the repeated poster reload loop.
- Optional per-ABI Lite APKs leave the P2P/torrent engine out of the package and hide the P2P rows that cannot apply to that variant. In-app updates match the installed variant, so a Lite device is offered `-android-lite-armv7.apk` or `-android-lite-arm64.apk` by pointer width rather than the full APK.
- 720p rendering is preserved when GPU upscaling is unsupported, while compatible 4K panels can use app upscaling.
- Shared resume, Simkl, library, subtitles, catalog and format-row improvements apply here too.

## Known issues

- Samsung startup fixes (#624, #628, #629) still need confirmation on affected TVs; please report back if your TV still starts blank.
- The map retains review follow-ups for library synchronization (#639) and tunneled playback (#626). These notes describe the merged implementation; they do not replace that review or device validation.
- Authenticated custom poster services and continuous Samsung HLS playback still need device confirmation recorded in the local history.
- Older integrated issue-map entries include partial fixes, information requests and work already present in v2.0.7. They are accounted for below without presenting them as newly completed features.

## Community

Thank you to everyone who sent code, reported problems and tested builds.

**Contribution authors**

- @I0-oX — PR #632, the selectable player format row, its preview, Dolby symbol artwork and Settings regression adjustments. Integration retains Original as the default.
- @Namer03 — PR #638, the Samsung ASS decoder fix; PR #640, precise seeking, revised seek gestures, local network logging and source ghosting fixes.
- @Matis94 (Mateusz in git history) — the merged `dv7-dts-webos` contribution: optional webOS P7 to P8.1 conversion, bounded DTS recovery, and strict DNS/HTTP regression fixture follow-ups.
- @iqui27 (Henrique Rocha) — integration, follow-up fixes, shared platform work and release preparation.

**Issue reporters**

Thank you to the reporters of #621, #624, #625, #626, #628, #629, #630, #634, #637 and #639, and to the testers who supplied the playback, memory, artwork and synchronization reports. No reporter identity is inferred from a reply author.

## Pull requests

All PRs named by the local `v2.0.7..HEAD` history:

- **#632 — Player format row**, @I0-oX. Resolution, HDR, audio format, language and channels under the title; selectable styles and preview. Merge `c26398525`, integrated by `855414818`; follow-up `145d5d951` keeps Original as the default.
- **#638 — Samsung ASS decoder fix**, @Namer03. Integrated as `5fb8810aa`, merge `4aaca4de6`; the README and build-workflow deletions were excluded.
- **#640 — Precise Samsung seek, revised gesture, local network log and source ghosting fix**, @Namer03. Integrated as `f67359b37`, merge `178d3da67`, with integration-gate and log-sanitizer follow-ups `49714272a` and `b05a33bcc`.
- **webOS DV7/DTS contribution — `Matis94/dv7-dts-webos`**, @Matis94. Merge `f1ef16b7b`, integrated by `febd70a00`. The local history does not record a PR number for this contribution, so none is invented.

`NuvioTV#4141` appears as an upstream visual reference in PR #632's commit body; it is not another merged PR in this repository.

## Issues closed or addressed

**Fixes or features in this release window**

- **#649 — Lite APK request from @infojoe:** per-ABI Lite APKs without the P2P/torrent engine (`5d34d5c70`, `aecc7f971`, `2c7ef280d`).
- **#415 — Constant crashes on LG:** texture pressure now scales to total RAM, keeps sampled pressure until memory recovers, and protects visible artwork (`e626c1c24`). This continues the earlier low-memory fix without claiming every crash is resolved.
- **#621 — Distorted GIF avatars:** centered cover cropping preserves aspect ratio (`58ccca756`).
- **#624 — 2.0.7 broken on Tizen 9:** placeholder EGL surface fix (`f840b4037`); TV confirmation pending.
- **#625 — Live TV does not work:** LG startup no longer selects video before track discovery (`abe573043`).
- **#626 — Android TV/Google TV tunneled playback request:** optional toggle (`19e638ea1`), disabled by default; review/device follow-up remains recorded.
- **#628 — Infinite black screen at startup on Tizen 9.0:** same startup-surface fix as #624; TV confirmation pending.
- **#629 — Blank launch on Samsung Tizen 10:** same startup-surface fix as #624; TV confirmation pending.
- **#630 — Add-ons installed but catalogs empty:** bounded Tizen DNS retries and shared resolver cache (`25eac3a93`).
- **#634 — LG webOS 3:** the map links this report to poster refresh after metadata language changes (`21dc9f41f`); older SDL compatibility is also included (`7dc80ce7c`). This does not establish support for every older TV configuration.
- **#637 — Bold does not work for Arabic subtitles:** Regular-only fallback fonts honor the bold choice (`7c8f1ea73`).
- **#639 — Library randomly removes, restores and adds titles:** complete sync snapshots, removal evidence and pending local saves are preserved (`75a1c56ae`, `515408fe5`); follow-up review remains recorded.

**Earlier integrated entries retained by the map**

These are the remaining items marked `integrada`. Their mapped fixes predate this release window or their outcome remains partial; they are included for a complete accounting, not as new 2.0.8 promises.

- **#246 — HEVC anime seeking:** the earlier resume-seek guard is integrated. The map explicitly says it does not prove manual HEVC seeking is fixed.
- **#313 — DTS to AC3 5.1 downmix:** earlier conversion is on webOS; the map says this does not fulfill Samsung downmix support.
- **#315 — AVI media cannot open:** mapped to earlier `agente/205-tpkfim`; the map retains a device verification step.
- **#337 — Hide the skip-intro button:** mapped to earlier `agente/205-skip`; no new commit for this request appears in the 2.0.8 window.
- **#343 — anime-skip.com suggestion:** earlier skip work is tracked; the map distinguishes existing AniSkip/TheIntroDB support from the requested service.
- **#346 — Continue Watching unselectable:** mapped to earlier Android bug work; device verification remains recorded.
- **#349 — MP4/P2P source preference:** mapped to earlier source-selection work; no new delivery is inferred from the status alone.
- **#357 — Zidoo/Ugoos crashes and missing sources:** earlier Android work is mapped, with affected-device confirmation still required.
- **#396 — C, Rust, roadmap and Discord:** informational issue; Discord is available at the permanent community link. This is not a Rust migration announcement.
- **#398 — Subtitle outline transparency:** earlier subtitle work is mapped; the map does not prove the requested 10% opacity steps were delivered.
- **#399 — Audio decoder selection and TrueHD 7.1 on Android:** earlier Android work is mapped; existing FFmpeg decoding does not prove the requested selector or affected passthrough case is resolved.
- **#401 — Slow Homebrew updates:** earlier investigation is retained. No new Homebrew distribution or measured installation-time claim is made here.
- **#404 — Continue Watching mixes titles:** earlier Samsung UI work is mapped; affected-case verification remains recorded.
- **#428 — Constant freezes:** earlier rest-screen crash work is mapped; no blanket claim about all freezes is made.
- **#450 — PS5 port:** informational tracking only; no PS5 implementation is evidenced by this release history.
- **#451 — Continue Watching synchronization:** earlier account progress work is mapped; the map identifies a progress/list discrepancy rather than proving a subtitle fix.
- **#460 — Reload and next-episode playback buttons:** earlier playback work is retained; no new button layout is delivered in this window.
- **#467 — AniList watch tracking:** metadata/art support is recorded, but the map does not establish AniList authentication or watch-history writes.
- **#469 — Samsung aspect ratio controls:** earlier aspect work is mapped; the reported Tizen 5.5 case still requires a log and device verification.
- **#474 — Top-right bar unreachable:** earlier top-menu work is mapped; the latest-fix review follow-up remains recorded.
- **#482 — Continue Watching leaks between profiles:** earlier profile-isolation work is mapped; no new profile-isolation change is attributed to this release.
- **#549 — See All for Showcase rows:** already integrated before v2.0.7 through `4baf783fa`.
- **#562 — Reload button:** already addressed before v2.0.7 by PR #574 (`2c7c85860`, `6a6d5b71b`).
- **#570 — Changing episode source shows an older movie and resets resume:** earlier triage work is mapped; device verification remains required.
- **#577 — Low FPS on Tizen 6.5 after 2.0.6:** earlier triage remains mapped; this release adds frame-pacing work without claiming a measured result on that TV.
- **#582 — LG stuttering and freezing:** earlier triage remains mapped; this release adds texture-pressure and adaptive-performance fixes without claiming every reported stall is resolved.
- **#606 — Subtitle filenames from add-ons:** earlier subtitle work is mapped; provider and synchronization labels receive further fixes in 2.0.8.

## Commits since v2.0.7

The complete local range through `ca4f44ae7`, including community commits, `agente/*` integration merges, test-harness repairs and issue-map updates, follows. The release-preparation commit itself is not included in its own list.

- `ca4f44ae7` Merge agente/205-205-testes208
- `0245c73fd` Merge agente/205-205-rel208
- `49941d939` test: align release card assertions with 2.0.8
- `5fbdaf20e` Merge agente/205-205-apklite
- `2c7ef280d` fix: verify expected ABI libraries per release APK
- `eb3385718` Merge agente/205-205-webosmem
- `e626c1c24` fix: stabilize webOS texture memory pressure
- `a3cef4262` test: remove duplicate live source stub from AIO instance fixture
- `aecc7f971` fix(android): preserve Lite updates with per-ABI APKs
- `67ba28ff4` test: retain explicit animation and episode glass choices in fixtures
- `9185c1b6c` test: preserve explicit solid guide overlay settings across migration
- `8a1170b17` test: preserve explicit glass settings in saved tab fixtures
- `51d171e8b` Merge agente/205-205-buscaarte
- `fd1180d71` test: supply session ownership and source age fixture boundaries
- `1214db978` test: align Simkl resume fixtures with numeric anime and watched ordering
- `6257ef79e` test: restore Seekr metrics and live badge fixture stubs
- `0e2c5dda0` test: supply fixed profile sessions for network probe fixtures
- `ae30aeb57` test: restore embedded text subtitle provider boundary
- `e9ed26a3b` test: restore Android player and prepared source fixtures
- `adfbb3d0c` i18n scan: allow legsync log-only autoEncerrar reasons
- `ecb6ef57a` test: restore plugin parsing and Plex subtitle dependencies
- `8f9da57cf` test: restore plain TV discovery and fresh guide pagination fixtures
- `3fedb9910` test: repair P2P fixture source and storage signatures
- `1bed62d8f` Merge agente/205-205-escada3
- `3dec05f8f` Merge agente/205-205-legref
- `71c29fcd1` test: capture addon ownership in manifest and profile fixtures
- `c49e9163d` test: update provider and subtitle harness contracts
- `3d7353282` test: restore extracted row and source worker boundaries
- `95ab52571` test: restore source and subtitle fixture dependencies
- `c9a0e1d1e` test: refresh episode and personal server fixture contracts
- `ed569db86` test: update episode capacity text input and Discord fixtures
- `a8068c440` test: use real settings collaborators for credit persistence coverage
- `f5a13f154` fix: resolve Search backdrops and fall back to TMDB title logos
- `5041945af` test: refresh collection discovery and control center navigation fixtures
- `fb98d2e8b` test: update stale library language and keyboard expectations
- `8b0786ebe` test: restore provider sync and extracted callback stubs
- `e9bee674b` test: repair catalog and home module fixture linkage
- `2e4b91a22` Merge agente/205-205-uxdados
- `c215a33c1` test: align settings fixtures with account backup and IPTV flows
- `0ad243e3e` fix: enable automatic-mode prerequisites from settings rows
- `7ab0ccf24` test: refresh search playback and calendar fixture dependencies
- `20e5dc16f` test: restore isolated rendering and storage fixture boundaries
- `48eac22ab` fix: reject transient GPU slowdowns and advance one rung at a time
- `cce2cc2fa` fix: cache session subtitles and preserve reference cancellation reasons
- `db19cc44b` test: bound suite runtime and refresh addon discovery fixtures
- `8f2b0548e` test(settings): expect four source priority options
- `855414818` Merge agente/205-205-pr632: PR #632 format row under the player title (Original stays default)
- `145d5d951` player format row: keep the Original style as the default
- `5d34d5c70` feat(android): add signed lite APK without embedded P2P engine
- `c26398525` Merge PR #632 (I0-oX): format row under the player title
- `fdaed2485` feat(ajustes): refine preview scene for Formato no player
- `8a46121a8` docs(map): sync tracking
- `d023e32ef` docs(map): sync tracking
- `f2c4faea7` refactor(ajustes): move the format setting's local-only case
- `069661178` refactor(ajustes): move the format setting's cases away from the pause-panel ones
- `8cd5499d9` refactor(ajustes): place the format setting away from the pause-panel entries
- `c22f4033f` test(ajustes): stop pinning the end of the settings list
- `33a52243f` fix(player): crisp Dolby symbol (re-traced, 96 px, drawn at 26)
- `966c0d91a` feat(player): format row under the title with a selectable style
- `0d33a122b` docs(map): sync tracking
- `0870b8caa` docs(map): sync tracking
- `58342b5ce` docs(map): sync tracking
- `d1563d5b3` docs(map): sync tracking
- `cebe243ac` docs(map): sync tracking
- `88f1a6d8a` docs(map): sync tracking
- `1a9cf77eb` docs(map): sync tracking
- `82f8b16a9` Merge agente/205-205-logrede
- `afcf66f36` Merge agente/205-205-bib639
- `515408fe5` contalib: weak salvos_remover_remoto; keep local mirror on pending '+'
- `b05a33bcc` fix(logrede): strip userinfo, non-:// schemes and long tokens in LAN log sanitizer
- `1e9121e7f` docs(map): sync tracking
- `b02444677` Merge agente/205-205-ltvcnc
- `178d3da67` Merge agente/205-205-pr640
- `d4d850204` Accept empty streams array as a valid add-on response
- `a580c7eae` docs(map): sync tracking
- `75a1c56ae` fix(library): preserve complete sync snapshots and removal evidence
- `96c2ba90e` arm.sh: build the DV7 converter on the host; the container has no cargo
- `49714272a` fix: align PR 640 integration gates with LAN logging
- `f67359b37` Tizen: precise seek, reworked seek gesture, local network log, source ghosting fix (#640)
- `1890f9f67` Merge agente/205-205-amp720
- `a5f0675e5` Fix long live TV stream requests and obsolete channel cap
- `e85d5a89d` fix: preserve 720p surface when GPU upscaling is unsupported
- `324182411` docs(map): sync tracking
- `59633662a` Merge agente/205-205-escada
- `c271714aa` fix: preserve manual effects when resetting adaptive reductions
- `8c083d04c` fix: enable app upscaling for 720p UI on 4K panels
- `0b7036d03` fix: require measured gains for automatic effects reductions
- `7a13a6bb3` Merge agente/205-205-semaddon
- `cac8be406` Merge agente/205-205-dtsrecup
- `5eb14e909` Merge agente/205-205-fundohome
- `136a2a21d` Merge integracao/2.0.5 and preserve extra-provider diagnosis
- `d5e0185cc` fix(dts): decouple webOS recovery from P7 conversion
- `4c7b5e47a` fix: order automatic glass frost and animation throttling
- `4befffb89` fix(home): resolve stage backdrops before visiting detail
- `80a3b437b` docs(map): sync tracking
- `433b0f00a` i18n scan: mfLigar reason is log-only (#638)
- `8d57ac57c` Merge agente/205-205-trimloop
- `4aaca4de6` Merge agente/205-205-pr638
- `45ba2392f` Merge agente/205-205-subsync (subsync2: provider labels in subtitle menu)
- `1aedd966b` Fix artwork reload loop: ignore background trim levels while resumed
- `5fb8810aa` tpk: keep the TV's subtitle decoder off ASS tracks to stop playback stalls (#638)
- `bfd9c6b38` Merge agente/205-205-gifavatar
- `d1217bfc7` Merge agente/205-205-epescala
- `9f9ac0eb8` fix: show provider labels in subtitle menu
- `da7b893fc` Merge agente/205-205-bufpiso
- `febd70a00` Merge branch 'agente/205-205-dv7matis' into integracao/2.0.5
- `15f073d74` Merge agente/205-205-subsync
- `9b7b08d42` Merge agente/205-205-arabbold
- `4be3db55c` Merge agente/205-205-fpstodas
- `3dd8c4d91` Merge agente/205-205-onepiece
- `84d89ab0d` fix: lower buffer safety floor and show available playback RAM
- `58ccca756` fix: preserve GIF avatar aspect with centered cover cropping
- `612a2ecd2` docs(map): sync tracking
- `0f1afc02d` Merge agente/205-205-perfauto
- `16b1cf5d0` Merge agente/205-205-imgloop2
- `3d19bb70d` fix: fit episode action menu and ratings within scaled screen
- `30142a7e6` fix(subtitles): preserve sync labels and skip informational tracks
- `10096c8ca` fix(player): share UI pacing and retain slow frame measurements
- `bfcd991a2` fix: preserve episode catalog when file lists exceed capacity
- `087d3a015` fix(dts): gate timestamp recovery on DV7 opt-in
- `7c8f1ea73` fix: honor subtitle bold with Regular-only Arabic fallbacks
- `b7d7ed0e8` fix: skip blocked texture uploads so smaller artwork can load
- `fcdae1879` fix: default performance settings to automatic and migrate saved choices
- `c9e4f8038` docs(map): sync tracking
- `9c2f06628` Merge agente/205-205-tunnel
- `cfa61591a` Merge agente/205-205-imgloop
- `a23aa5903` Merge agente/205-205-aiomulti
- `de19d7684` Merge agente/205-205-topx
- `ee38bf01f` Merge agente/205-205-plugmeta
- `e01a8e04a` Merge agente/205-205-playerfps
- `657e9c012` Merge agente/205-205-livetv720
- `19e638ea1` feat(android): add opt-in tunneled playback setting
- `82ee52a2b` fix: keep AIOStreams instances distinct by transport URL
- `c95fa330a` fix: preserve root catalog identities in manifest parsing
- `55d077c80` fix(tex): defer uploads when visible artwork fills the budget
- `884609d90` fix(streams): anchor names and retain title alongside description
- `2206b5a27` fix: bound browser frame waits while player UI is drawn
- `76cd556a1` fix: preserve live TV resolution when account is full
- `845fe8cb1` fix: require DV7 opt-in for P7 conversion and recovery
- `84eae41cf` Merge agente/205-205-fpsoverlay
- `564b557e2` Merge agente/205-205-posterpt
- `609ed3cc3` docs(map): sync tracking
- `ecad7d1e2` Fix Live TV Automatic HLS quality and unreliable speed advice
- `15af443d8` fix: label performance overlay frame rate as UI fps
- `ad50fc8f5` Merge agente/205-205-tpkpausa
- `0f18626ad` Merge agente/205-205-simklanime2
- `f1ef16b7b` Merge Matis94/dv7-dts-webos (DV7 to P8.1 and DTS recovery on webOS)
- `5fd18864f` fix: disambiguate URL log macro for strict DNS regression build
- `83072bc99` test: supply official HTTP buffer dependencies in webOS fixture
- `21dc9f41f` fix: invalidate catalog poster cache when metadata language changes
- `a4d52842e` Merge agente/205-205-trailerquad
- `d4f8c2845` Merge agente/205-205-idiomateste
- `5bc056384` fix(simkl): accept numeric anime next episodes from cached bodies
- `6755ff560` fix(tpk): suppress startup notices after normal background eviction
- `ad6ec6fb9` docs(map): sync tracking
- `d072c891b` Merge agente/205-imgbaixa
- `c450d139d` Merge agente/205-205-posterserv
- `8271a8dbb` fix(home): keep card trailer placement in its own coordinate space
- `1998c125f` fix: retain DV native-path reason and compile legacy recovery fixture
- `9c5062588` webOS: isolate bounded DTS timestamp and HTTP range recovery
- `a790e161c` webOS: add independent opt-in Dolby Vision P7 to P8.1 conversion
- `3b3906186` Fix language test coverage and restore i18n checks
- `98ed41d91` fix(i18n): restore sorted translation table order
- `05a3fd630` Merge integracao/2.0.5 and preserve both C9 test modes
- `89b48afe7` docs(map): sync tracking
- `a25def053` fix: unify poster coverage and support shape templates
- `eb5203637` Merge agente/205-205-scrobblept
- `fcc581387` Merge agente/205-205-apkpoda
- `bf6df9a8e` Merge agente/205-205-diaglimitada
- `ceca13b7e` fix: keep tracker playback active during checkpoints
- `70b1f55e3` fix(android): prune superseded update APKs
- `9e6c12d47` Merge agente/205-205-pontodesliga
- `a1169f5ad` Merge agente/205-205-armazenamento
- `6c8233ded` Merge agente/205-205-bufferhttp
- `6a392a97f` fix(livetv): run explicit diagnostics on limited accounts
- `35caf5df8` Merge agente/205-top10-deitado
- `6c9bc485b` fix(webos): apply buffer presets to direct HTTP VOD
- `5623ab442` fix: persist playback checkpoints across TV power loss
- `12bedced8` docs(map): sync tracking
- `d35f60069` fix(android): preserve durable storage and APK staging
- `5d30131d6` Merge agente/205-205-simklcw
- `af9250ab5` fix(simkl): map absolute anime episodes using provider metadata
- `e731a7847` fix(android): restore APK cache routing and viable P2P window
- `e3cd59e71` Merge agente/205-205-tpkzoom
- `f3f5af5ff` Merge agente/205-205-tpkfps
- `1d2ab0a5b` fix(simkl): retain absolute anime episodes and fallback IDs in continue watching
- `c3ac7d1ae` fix(tpk): reset live geometry and settle guide transitions once
- `d8699b55c` fix(android): bound regenerable storage and add clear cache action
- `3e709ec43` Merge agente/205-205-simklbib
- `b3c4a5e36` fix: keep API11 window rendering over static video holes
- `0a1a32a13` Merge agente/205-205-p2pcauda
- `b206981ce` Merge agente/205-205-livehls
- `30efe6903` fix: retain complete Simkl library lists across all statuses
- `f215e7fd8` Merge agente/205-plugins-fila
- `8847255a3` fix(tpk): skip competing media probes during live playback
- `d4cb60161` fix(dts): bound optional P2P tail probes during startup
- `83fe3c59a` fix: recognize local plugins in empty source diagnosis
- `009bba7ae` docs(map): sync tracking
- `921210932` Merge agente/205-205-sdlantigo
- `7dc80ce7c` fix: keep text surfaces compatible with SDL 2.0.4
- `dda4428e4` Merge agente/205-205-livets
- `b22929845` Merge agente/205-205-fimvod
- `beb30e5b9` Merge agente/205-205-dnslento
- `25eac3a93` fix(network): bound Tizen DNS retries and share resolver cache
- `abe573043` fix(webos): start playback without premature video track selection
- `06a178e07` fix(tpk): preserve controls and episode completion during stalls
- `f1743efe5` docs(map): sync tracking
- `cfe0215b4` fix: prioritize useful plugin scrapers and report empty rounds
- `b19d8ec28` fix(home): keep ranked row artwork in portrait format
- `e901b95e7` docs(map): sync tracking
- `8e67f7be0` fix(p2p): play on TVs with ~460 MB free by allowing a 160 MB window
- `36a2d1f13` fix(tpk): no 'Previous launch stopped' notice after a normal TV close
- `b84091f4c` fix(tizen-wgt): stub rede_baixar_trecho64_final in the browser build
- `9460c0dd8` docs(map): sync tracking
- `dacee39da` docs(map): sync tracking
- `f840b4037` fix(tpk): ignore the 1x1 placeholder EGL surface at startup
- `d2a392e14` docs(map): sync tracking
- `bdc7d7c8c` test(gpu): verify native C9 artwork against pre-upscaler baseline

**Full comparison:** https://github.com/iqui27/nuvio-native-legacy/compare/v2.0.7...v2.0.8
