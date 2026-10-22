# Triagem 2.0.5 — English reply drafts

Não publicados. O dono revisa e posta. Sem datas prometidas. Base offline: snapshot de issues, mapa curado, commits e notas de release locais. Fechamento não equivale a confirmação em TV.

39 rascunhos de lançamento (7 NÃO FECHAR SEM DECISÃO), 18 pedidos de informação, 4 duplicatas/candidatas (1 confirmada, 3 a confirmar). #419 tem os dois textos na mesma seção; total: 61 textos, 60 issues únicas.

## A — Abertas com conserto lançado (39)

### #266 — Bug: Doesn't open on ATv

Versão/evidência local: 2.0.2; a1d5e039 (v2.0.2).

Thanks for the report! This change shipped in 2.0.2: Android 11 now has a rescue screen for a black screen at launch, with log sending available without login. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #283 — Minha tv lg nao consegue abrir nenhum canal dos meus addons

Versão/evidência local: 2.0.2; b68bad96 (v2.0.2).

Thanks for the report! This change shipped in 2.0.2: Add-on live channels can now open on LG. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #286 — Performance on older Samsung UA40N5300

Versão/evidência local: 2.0.2; e03080dd (v2.0.2).

Thanks for the report! This change shipped in 2.0.2: The Light effects setting drops to minimal on Tizen 4 TVs with Mali-400 GPUs, reducing the UI load. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #293 — Audio codec details on player

Versão/evidência local: 2.0.2; db1f5891 (v2.0.2).

Thanks for the report! This change shipped in 2.0.2: The player now shows the audio codec. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #310 — [port] regex/options for autoplay

Versão/evidência local: 2.0.2; release v2.0.2 (release-notes.md).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.2 shipped this part: Autoplay now supports allowed add-ons/plugins, a regex filter and an instant-wait option. Resolved-link caching and separate regex rules by content type remain outside that shipped change. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #326 — Add continue watching options

Versão/evidência local: 2.0.5; 1cc512dd (v2.0.5); 56868756 (v2.0.5); 95fc93c5 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Continue Watching has a configurable minimum progress threshold (1–20%). Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #328 — 🐛 Bug Report: Episodes Are Being Duplicated

Versão/evidência local: 2.0.3; 7edab4b0 (v2.0.3); 9c5d0027 (v2.0.3).

Thanks for the report! This change shipped in 2.0.3: Repeated episodes are removed from the episode list. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #329 — Add skip intro, recaps and credits.

Versão/evidência local: 2.0.5; 4193781f (v2.0.5); 95fc93c5 (v2.0.5); a0a392fb (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: There are options to skip intro, recap and credits when matching markers are available; credits need a real start and end marker. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #331 — 💡 Feature Request: Netflix-Style Next Episode Countdown

Versão/evidência local: 2.0.5; 11791eda (v2.0.5); 4193781f (v2.0.5); 4c17e680 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: The next-episode countdown is configurable and starts from a real credits marker, rather than the estimated last two minutes. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #334 — P2P filled the TV's free space" error when playing large/4K streams

Versão/evidência local: 2.0.3; c3e19d00 (v2.0.3); d3ca720b (v2.0.3).

Thanks for the report! This change shipped in 2.0.3: P2P settings now include a space limit, and leftovers from crashed sessions are cleaned at startup. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #338 — Add Stats for Nerds and Custom Buffer Size

Versão/evidência local: 2.0.5; 74f781f3 (v2.0.5); 8c856535 (v2.0.5); 95fc93c5 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Live playback statistics are available, including audio/subtitle details, and Android has a configurable playback buffer. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #341 — Aspect Ratio / Crop feature to fill screen does not work on Tizen 5 TVs 📺

Versão/evidência local: 2.0.3; release v2.0.3 (release-notes.md).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.3 shipped this part: A probable fix for the aspect/zoom button on Tizen 4/5 was shipped; it still needs confirmation on the affected TV. Cropping black bars encoded inside the video is a separate request, and the aspect fix needs TV confirmation. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #350 — Major bug

Versão/evidência local: 2.0.3; 4b8ba24b (v2.0.3).

Thanks for the report! This change shipped in 2.0.3: Continue Watching has a View details action, and Live TV reconnect no longer mistakes a provider ending the stream for a manual pause. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #352 — [Feature Request] Library Watched Indicators, Watched/Unwatched Sorting & Arabic UI Support

Versão/evidência local: 2.0.5; 402e2438 (v2.0.5); d4d72639 (v2.0.5); fab062b3 (v2.0.5).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.5 shipped this part: Library posters and rows have watched indicators, with watched/unwatched sorting. Arabic menus and larger library limits remain separate requests. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #355 — Option to view full/untruncated text and metadata for addon source streams

Versão/evidência local: 2.0.5; ac055d54 (v2.0.5); b176eff5 (v2.0.5); c8dbfda5 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: The source info button opens the full label, description and file name without truncating them. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #356 — Upcoming shows not appearing in continue watching

Versão/evidência local: 2.0.3; d47a78c8 (v2.0.3).

Thanks for the report! This change shipped in 2.0.3: Up Next checks Cinemeta before dropping an episode missing from the Nuvio catalog. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #358 — 🐛 Bug Report: Some Add-on Catalogs Are Incorrectly Moved to “Not on Home Screen”

Versão/evidência local: 2.0.5; 29c640b7 (v2.0.5); c4edb7c6 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: New add-on catalogs that do not fit on Home go to the queue instead of being switched off. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #362 — Collection View Options

Versão/evidência local: 2.0.5; 22320582 (v2.0.5); 491fcee3 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Collection display options were added, and Automatic uses the grid for collections with tabs; Showcase is selected explicitly through the display button. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #364 — Sources unable to open

Versão/evidência local: 2.0.5; 8f2c1701 (v2.0.5); bf666aa7 (v2.0.5); d78963ab (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: When the player rejects a source, automatic playback can try the next one instead of stopping at the rejected source. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #369 — [Bug] Multiple issues/Missing Features on Android TV version and Perhaps on other verisons too

Versão/evidência local: 2.0.3; 5f04c36b (v2.0.3); 712557ca (v2.0.3).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.3 shipped this part: Thai text uses a bundled font; Hide unreleased also covers Home rows and collection/View all grids. The other Android issues and feature requests in this report still need individual confirmation; these changes do not resolve the whole report. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #383 — [port] subtitle memory same as nuvio

Versão/evidência local: 2.0.5; 3654b88e (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: A subtitle selected manually is remembered for the next episode. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #385 — Internet Connection Drop and Reconnecting Bug to no end

Versão/evidência local: 2.0.3; 4151683c (v2.0.3); 9ee093ac (v2.0.3).

Thanks for the report! This change shipped in 2.0.3: Background subtitle, track and chapter readers back off after a video server refuses connections, instead of repeatedly retrying in parallel. A source can still fail if the server refuses the TV player too. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #387 — [bug] instant long press in library

Versão/evidência local: 2.0.5; 03213598 (v2.0.5); 0f98379f (v2.0.5); c82fa422 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Holding OK no longer triggers a long press immediately after a long pause in the event loop. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #390 — Custom poster source not setting

Versão/evidência local: 2.0.4; a7bb794c (v2.0.4); a8fcd146 (v2.0.4); adfba576 (v2.0.4).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.4 shipped this part: The initial custom poster-template saving fix shipped, preserving longer input and its case. Version 2.0.5 also adds placeholder aliases and immediate rejection feedback. Please confirm that your template saves and is applied; the owner still needs to decide whether any remaining poster-provider behavior belongs in a separate issue. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #392 — Profile switching still rearrange my home screen

Versão/evidência local: 2.0.4; 1d4b5e97 (v2.0.4); 2d3e472e (v2.0.4); 9023ebdb (v2.0.4).

Thanks for the report! This change shipped in 2.0.4: Switching profiles no longer registers the previous profile’s catalogs into the new profile’s Home order. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #410 — UI appearence issues

Versão/evidência local: 2.0.4; 52dbde71 (v2.0.4); b32c9410 (v2.0.4); b8935ecf (v2.0.4).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.4 shipped this part: UI effects received rendering corrections. Sharpness and 4K output still need confirmation on the affected TV. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #416 — [Samsung Tizen 6 / v2.0.4] Filmography page empty - every api.themoviedb.org fetch fails with network error

Versão/evidência local: 2.0.5; 4b43829a (v2.0.5); 7f338254 (v2.0.5); 8b5ed6c0 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Metadata can load through a fallback when the main TMDB address is blocked or times out. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #417 — [Samsung Tizen 6 / v2.0.4] Every card in the Trailers row plays the same video

Versão/evidência local: 2.0.5; e0bb1020 (v2.0.5); f219bc9d (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: The title page now shows one trailer card, rather than several cards for the same video. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

<a id="issue-419"></a>

### #419 — [Samsung Tizen 6 / v2.0.4] Clicking a Recommendations card does nothing

Versão/evidência local: 2.0.5; 7f338254 (v2.0.5); 8b5ed6c0 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Metadata can load through a fallback when the main TMDB address is blocked or times out, covering the recommendation navigation path. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

**Alternativa C — SUSPEITA, confirmar antes de tratar como duplicata de #416:**

Thanks! Your report mentions TMDB requests failing, which may connect this to #416, but the symptoms are different. On 2.0.5, do recommendations still fail to open, and does the filmography page fail at the same time? Please confirm whether both happen together before we combine the reports.

### #421 — Bold and style in Arabic subtitles still not working

Versão/evidência local: 2.0.5; aced3443 (v2.0.5); c8e8d983 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Embedded text subtitles are rendered by the app on LG and Samsung, so the app can apply subtitle styling. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #422 — In-app screensaver comes on during an autoplaying trailer if I don’t touch the remote

Versão/evidência local: 2.0.5; 65bb6e2c (v2.0.5); a408e51a (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: The in-app screensaver waits while a trailer is playing. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #425 — Unnable to remove subtitle languages

Versão/evidência local: 2.0.5; 2b4119d5 (v2.0.5); 2c571ab6 (v2.0.5); 8b7468b0 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: The subtitle panel has direct access to recommended rows and a Show only my languages option, with secondary-subtitle actions in its side menu. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #426 — Aspect ratio bug

Versão/evidência local: 2.0.5; 3ba2987a (v2.0.5); c4f344e8 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Samsung aspect changes keep the picture in place, refuse an off-screen crop, and restore sound after a refused or queued change. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #427 — Memory for images capped at 160 MB

Versão/evidência local: 2.0.5; 92ee6141 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Settings now distinguish the saved image-memory choice from the limit applied on the TV. This explains the cap; it does not increase the TV’s available RAM. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #429 — Subtitles auto select

Versão/evidência local: 2.0.5; 2d42c8e0 (v2.0.5); b1da4311 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Serbo-Croatian matches Croatian, Serbian and Bosnian, with an exact language match preferred for automatic selection. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #430 — Multiple intro skip sources and priority order

Versão/evidência local: 2.0.5; 551edb43 (v2.0.5); a9c97b26 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: AniSkip markers also work for anime opened through IMDb/TMDB IDs, across seasons. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #433 — All posters are displayed horizontally, not coming from my addon

Versão/evidência local: 2.0.5; 7777e129 (v2.0.5); ea0c322a (v2.0.5).

**NÃO FECHAR SEM DECISÃO**

Thanks for the report! Version 2.0.5 shipped this part: Long poster addresses are accepted and the horizontal fallback was corrected. The Continue Watching part of the report remains unconfirmed; please also confirm the poster shape and origin on your TV. We are keeping this open for the remaining scope. If this is later closed and the shipped part still fails on that version, please reopen it with the details.

### #439 — Increase maximum subtitle size from 250 to 300

Versão/evidência local: 2.0.5; 2f1e2de1 (v2.0.5); 6ad103f3 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Subtitle size can now be increased to 300%. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

### #440 — Request: Subtitle size decreases when Bold is enabled

Versão/evidência local: 2.0.5; 2f1e2de1 (v2.0.5); 6ad103f3 (v2.0.5).

Thanks for the report! This change shipped in 2.0.5: Arabic Bold is adjusted to match the regular subtitle size. Closing this for the shipped change; please reopen if the reported issue still happens on that version.

## B — Precisa informação do autor (18)

### #288 — Profile selection background is grainy dark video

Thanks for the details! Could you confirm the platform/package you installed? The report says LG webOS, but the model is TCL C6K. Please share the exact app version you test now and a log code after opening one of the affected profile backgrounds on 2.0.5. You can get the code from Settings > Send log (under About and help).

### #315 — .avi media files fail to play ("Could not open the source")

Thanks for the details! Could you retry the same AVI on 2.0.5 and share the code from a log sent immediately after “Could not open the source”? If available, include the video/audio codec names, without sharing the stream link. You can get the code from Settings > Send log (under About and help).

### #357 — Nuvio v2.0.1/2.0.2 – Crashes on Zidoo Z9X 8K and Ugoos AM9 Pro + Missing Source Options

Thanks for the details! Could you retest the Zidoo on 2.0.5 and share its exact firmware version and an Android logcat captured at launch? If it closes before Settings, logcat is the useful capture here; please remove account details and stream links before attaching it. On the Ugoos, does the gray bar still appear on 2.0.5, and can you show a photo?

### #379 — Movie or TV show at the end never return to homescreen.

Thanks for the details! On 2.0.5, does this happen after a movie, an episode with another episode available, or both? Please send a log right after it stays on the last frame and share the code. You can get the code from Settings > Send log (under About and help).

### #382 — No internet error despite having internet

Thanks for the details! When “No internet” appears after a cold boot, please send a log and post its code. Is the TV on Wi-Fi or cable, how long does the warning keep returning, and which app version are you testing now? You can get the code from Settings > Send log (under About and help).

### #388 — Profile picture on side bar is squashed

Thanks for the details! Thanks for the photo and the HTTP 401 detail. Which app version shows that log-sending error, and was the custom avatar uploaded to your Nuvio account or chosen on the TV? If Send log succeeds on 2.0.5, please post the code; if it still returns 401, just tell us that result. You can get the code from Settings > Send log (under About and help).

### #398 — Subtitle outline transparrency

Thanks for the details! Could you attach an example of the outline problem here, with the subtitle font, opacity and border settings? Please say whether it is an embedded or add-on subtitle and whether it is SRT or ASS, then share a log code after reproducing on 2.0.5. The linked examples are not included in this report. You can get the code from Settings > Send log (under About and help).

### #401 — Long update time on Homebrew channel (15 mins +)

Thanks for the details! Could you time each Homebrew Channel phase separately—Downloading, Verifying and Installing—and tell us the version you are updating from and to? Thanks for confirming that the in-app update to 2.0.4 was near instant; we need to distinguish that from the slow Homebrew upgrade.

### #404 — Continue watching mixes up

Thanks for the details! On 2.0.5, which Continue Watching source is selected in Settings, and are Trakt or Simkl connected? Please describe how one unplayed title first appears, confirm whether other devices use the same Nuvio profile, and send a log afterward with its code. Please do not share account credentials or source links. You can get the code from Settings > Send log (under About and help).

### #407 — Top menu open when scrolling up on the title page

Thanks for the details! Could you confirm whether pressing Up above Play still opens the top panel on 2.0.5, and whether you mean the social/saved panel or the main side menu? Please send a log after that key sequence and share its code. You can get the code from Settings > Send log (under About and help).

### #414 — Collection does not load

Thanks for the details! Could you share your exact Samsung model and Tizen version, then retry on 2.0.5? Please describe the steps from opening the collection to pressing “OK to load”, and share a log code right afterward. No collection URL or credentials are needed. You can get the code from Settings > Send log (under About and help).

### #415 — constant crashing, no matter if I play or not

Thanks for the details! Could you reproduce once on 2.0.5 and send a fresh log after reopening the app, then share the code? Please tell us whether it was browsing or starting playback, the resolution if playing, and the exact image-memory/buffer settings. The earlier logs do not establish whether this version changes the result. You can get the code from Settings > Send log (under About and help).

### #418 — App fails to connect: "The Nuvio account server didn't respond" and QR code does not generate

Thanks for the details! Could you share the exact TV model and retry QR/manual login on 2.0.5? If you can reach Send log, please send it after the failure and post the code; if login prevents reaching it, tell us that instead. Do not share your password or login token. You can get the code from Settings > Send log (under About and help).

### #423 — Not update LG

Thanks for the details! Could you share the exact LG model, webOS version and whether you updated inside Nuvio or through Homebrew Channel? Please tell us the version before/after restarting and which phase gets stuck. If the app still opens, send a log and post its code. Did a reboot and LG memory cleanup help on your TV too? You can get the code from Settings > Send log (under About and help).

### #424 — Live tv not loading categories

Thanks for the details! On 2.0.5, roughly how many Xtream categories/channels are expected, and how many appear? Please name one missing category and describe how you open the list, then send a log and share its code. Please leave out the server address and login details. You can get the code from Settings > Send log (under About and help).

### #428 — Constant freezes

Thanks for the details! Could you post the log code shown after Send log? The time alone cannot identify the right log. Please also tell us whether the same menu/screensaver freeze happens on 2.0.5 and which steps trigger it. You can get the code from Settings > Send log (under About and help).

### #451 — Continue watching subtitles not syncing

Thanks for the details! Could you share the exact Legacy version and confirm that every device uses the same Nuvio profile? Please name one missing series/episode and describe the steps from playing it on another device to refreshing Continue Watching here, then send a log on the Samsung and post its code. You can get the code from Settings > Send log (under About and help).

### #468 — All Jellyfin catalogs do not show in catalogs list

Thanks for the details! Could you send a log after refreshing the Jellyfin catalogs and post its code? Please list the names and total number of catalogs you expect, which ones appear, and whether the missing ones contain movies, shows or both. Please do not include the server address or credentials. You can get the code from Settings > Send log (under About and help).

## C — Duplicata confirmada e candidatas (4)

### #330 — Confirmada → #412

Thanks for reporting this! #412 replaces this report with a clearer reproduction of the same slowdown. Closing this duplicate so the investigation and follow-up stay together in #412. Please add any further results there.

### #391 — SUSPEITA → #372

Thanks! This may overlap with #372, but we have not confirmed that they are the same problem. Which series and metadata add-on show the missing seasons/episodes, and does the mismatch match the season numbering described in #372? Please compare on 2.0.5; we already have your log code Z4HDY2.

### #365 — SUSPEITA → #254

Thanks! #254 describes stale Home catalogs, while your report says Home is fixed and only “Not on Home Screen” keeps old entries. Is that still the distinction on 2.0.5 after removing an add-on and refreshing? Please confirm before we combine these reports; we already have VK50RJ.

**#419 → #416:** texto de confirmação na [seção #419](#issue-419); candidato, não fechar como duplicata sem confirmação.

## #437 — CloudStream (fechar, fora do escopo; dono decidiu 12/10)

Thanks for the suggestion. We looked into it and decided not to support CloudStream repositories in Nuvio Legacy:

- CloudStream extensions are compiled Kotlin plugins for the Android JVM. Our TV app is native C, so they can't run here without rebuilding a whole plugin runtime.
- Most of those repositories scrape third-party sites. Bundling that kind of source inside the app is a legal risk we can't take on for the project.
- If you want extra sources, Nuvio-format JS plugins and regular Stremio addons already work in the app.

Closing as out of scope. Thanks again for taking the time to write it up.
