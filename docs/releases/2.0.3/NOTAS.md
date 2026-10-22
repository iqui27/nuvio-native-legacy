# Nuvio Legacy 2.0.3

A release focused on fixes and optimization: Dolby Vision in MKV on LG TVs (off by default), Settings redesigned, the Magic Remote pointer on almost every screen, a new episode menu and a long list of fixes.

## Added

- **Settings redesigned.** Categories by task (Watch, Screens, Account and system), plain-language labels, a written On/Off state on every switch, and the technical options behind "More options" in each category.
- **A preview scene for each option**, so you see what it changes before you change it.
- **Settings layout: Panel or List** (#339).
- **Search options** (#311): Nuvio catalog results first, last or off; hide the "from <source>" caption. The Cinemeta switch only shows when it can do something.
- **Next episode in Continue** can be turned off, and a single up-next card can be removed (it stays hidden for that profile and on Trakt).
- **"View details"** in the Continue Watching hold menu, when OK on the card plays (#350).
- **New episode menu.** Holding OK on an episode opens the same menu as posters, beside the card. Holding OK on a season tab opens a season panel.
- **Episode ratings** show the Trakt or TMDB logo next to the score. The episode synopsis is smaller.
- **TV guide:** choose which channel add-ons show up, per profile (#283). When a provider has more channels than fit, the guide says "Showing N of M channels" instead of dropping categories.
- **P2P space limit** in the P2P settings (#334). Leftovers from a session that crashed are deleted at start.
- **LG: Dolby Vision in MKV** shows a screen while it opens, with each step and a "Watch now in HDR10" button. If the audio is TrueHD, it switches to E-AC-3 or AC-3 in the language you were listening to, so Dolby Vision stays on; if it can't match the track safely, it doesn't switch. A failed header read is retried instead of falling back to HDR10, and the film still starts where you pressed Play. If it falls back to HDR10, it continues from that point. The option is still off by default.
- **LG Magic Remote pointer** on almost every screen: Library, Settings, Explore, View all, Search, Profile, Saved, Agenda, TV guide, Add-ons, the player sheets, the post-play screen and the Dolby Vision screen (#99).
- **Thai** is drawn with a bundled Noto Sans Thai font, in the interface and in subtitles (#369).
- **Hide unreleased** now also applies to Home rows and to collection and View all grids (#369).
- **MKV chapters** on Android and Samsung .tpk are used for intro, credits and preview markers.
- **Skip intro and next episode** use markers checked against the episode, TMDB numbering first, AniSkip for anime, and a timed fallback.
- **Simkl sign-in** shows "Expires in m:ss" and no longer rejects the code on a slow or empty reply.
- **Subtitle size up to 250%** (#335).
- **Support the project** shows the Patreon and Ko-fi QR codes again.
- The usage guide now covers 2.0.

## Changed for everyone

- **Settings size** goes from 90% to 80% once, for those still on the old default. 100% and later changes stay.
- **Hero trailer** does not play by itself on TVs with a Mali-400/450/470 GPU. The setting is kept and the title page trailer still works (#286).
- **Add-ons disabled on the account** are no longer queried on the TV: catalogs, search, View all, guide, sources and subtitles.
- **Higher limits:** account add-ons 32 to 64, add-on subtitles 12 to 36 (12 per language), Android TV guide 3000 channels and 128 categories (TVs stay at 900 and 48).
- **End-of-video card** without an accepted marker: series at 50 s before the end (was 40), movies at a fixed 3 min, 90 s under 1 h, none under 10 min.
- **Holding fast-forward** speeds up and crosses the whole file in about 10 s. A single press is still 10 s (#340).
- **Recommendations from friends** no longer open over Home. The clock island announces them and the card opens when you choose it.
- **LG, automatic source pick:** with autoplay on, HDR set to Prefer and Dolby Vision on, an MP4 comes first among sources of the same resolution (MP4 Dolby Vision, then MP4 HDR, then MP4 SDR), even ahead of Dolby Vision in MKV. It never drops to a lower resolution to get an MP4, and your source rules still come first.

## Fixed

- **Live TV:** the reconnect watcher took the provider's end of stream for a manual pause and never reopened the channel. Manual pause is now tracked on its own (#302, #350).
- **Samsung .tpk with Mali-400 (Utgard):** poster mipmaps are built on the CPU and the hero image is decoded at 960 px, so Home stutters less (#286).
- **Search** waits 300 ms after the last key before asking the add-ons, and keeps the previous rows until the first reply (#368). A search with no results clears the previous ones.
- **Add-on sync** merges your TV edits with the account instead of replacing it: an add-on installed on the phone stays, one removed on the phone does not come back. The merged list is never cut. After 3 refusals (4xx) the TV edit is dropped and the account list is applied (#360).
- **Fixed sidebar:** the row editor (Reorder and enable rows) and the View all and collection grids start after it instead of under it (#359).
- **Up next** no longer disappears when the Nuvio catalog does not list the episode; Cinemeta is checked first (#356).
- **Recommending a series** from a Continue Watching card works again (#363).
- **Samsung .wgt:** Arabic subtitles use Noto Naskh (#370). Holding OK in the subtitle sheet makes one choice instead of reloading on every repeat (#370).
- **Title carousel** (Dynamic layout) fetches the next and previous title's details and episode list while you rest on one, so they are ready when you get there. Going back to a title already seen asks for nothing.
- **Your subtitle choice comes back.** The subtitle you pick by hand (built-in or add-on) is remembered per profile: reopening the same title turns on that same track, other titles start with that language, and turning subtitles off by hand keeps them off. A subtitle language set in Settings still comes first for other titles.
- **Profile page:** Left at the edge opens the sidebar without closing the page, also on a new profile or one with only friends (#371).
- **Continue Watching:** with Episode thumbnail on, the card shows the episode still; a watched movie says Play. With the Nuvio account as source, up next is filled even when Trakt or Simkl is linked, without episodes already watched elsewhere.
- **Watched:** "up to here" and "season" only send episodes that change, so Trakt gets no duplicate plays. The main button, next episode and "% watched" follow the change right away. Marking a series as watched no longer hides it on Trakt; only "Remove from Continue Watching" does.
- **Unmarking an episode sticks:** an episode you unmark stays unmarked even when Trakt or the Nuvio account still lists it as watched, and after restarting the app. It comes back only if you mark or watch it again, or it is watched later on another device. The log now says which source marked each episode.
- **Continue Watching follows your unmarks:** when Trakt, Simkl or the Nuvio account say a series is further along than what you left marked here, the "up next" card now shows the first episode you unmarked (for example S2E7 instead of S3E1). It does not apply when the service has a newer watch than your unmark.
- **Episode list** missing on a title page, or no Up next card in the player, after browsing many titles.
- **Subtitles** move up only while the Up next card is on screen, and come back down when it hides.
- **Credits markers** are not accepted before the player knows the video length.
- **Home hero:** the logo arrives with the art, the next item is loaded ahead, and items from Trakt lists get their synopsis. The hero no longer turns while the profile picker is open. Switching titles quickly no longer loses a synopsis or crashes the app.
- **Home hero:** the previous and next hero art are now loaded in the background while you rest on one, so flipping the hero by hand (either direction) shows the art right away instead of waiting.
- **LG Dolby Vision:** no more pause panel while the stream fills, and playback starts at the saved point. Smoother background on OLED.
- **Android:** the saved aspect mode waits for the first frame, so Dolby Vision no longer opens dark.
- **Android:** recreating the video surface (HDR start, aspect change) no longer freezes the remote for seconds while the decoder lets go of it, which could end in "app not responding".
- **Speed test** keeps the byte range through debrid redirects, measures with 4 connections on Android again, and handles sources served as octet-stream.
- **Crashes from threads racing each other** (watched episodes, parallel source check, catalog episodes, sign-in token, collections) (#203, #323).
- **LG webOS 4:** startup crash from FreeType symbols, and startup crashes are now reported on the next launch (#317).
- **Android:** start hang on Shield and other boxes, sign-in through the system network stack when libcurl fails (#266, #332); MStar flicker fixes and a crash report on Android 11 and older (#318).
- **Home:** switching profile no longer shows the previous profile's rows (#294); the hero no longer depends on a drawn row, and a catalog added from Not on Home is no longer swallowed by a collection (#327); rebuilds are spaced out so Home settles with many collections (#319, #280).
- **Add-ons with very long URLs** (2 to 16 KB) are no longer ignored, and add-ons past the limit are logged instead of cut silently.
- **Auto-play** never picks an add-on placeholder; Samsung never picks Dolby Vision.
- **Best for this TV** shows the resolution in the add-on's own order too, and an unknown resolution shows as "?" instead of 0p (#284). On LG, Dolby Vision in MKV only counts toward this ranking when that option is on.
- **Embedded subtitles:** large MKV track headers, gentler reads, no stale downloads (#308, #330); more add-on subtitles (#268); Arabic subtitles with real bold and sharp ASS in 4K (#335).
- **Embedded ASS subtitles no longer stop after the opening** (#384). On releases with heavy karaoke and typesetting, the opening used up the 8000-line limit and the rest of the episode had no subtitles. The whole track is indexed now; when it is too big to keep in memory, the app keeps the part around where you are and fetches the rest as you get there.
- **Video server refusing connections** (#385). When the server of the video refuses a connection (seen on Samsung .tpk with TorBox links), the embedded subtitle reader, the track probe and the chapter reader stop and wait 10 to 60 s before trying that server again, with one connection at a time, instead of retrying several times while the video opens. It does not make a refused source play: if the server also refuses the TV player, the source still fails.
- **Duplicate episodes** in the episode list (#328).
- **LG:** resume no longer drops the source on a failed seek (#246); DTS audio on LG TVs from 2020 onward is now converted before playback (#285). Some 2023–2024 models could decode it natively; conversion is used for all of them because support can't be detected reliably.
- **Samsung:** Pause and subtitle list errors (#269); aspect/zoom button on Tizen 4/5 (#341, probable fix, not yet confirmed on a TV); Arabic subtitles when an older install left old fonts. When the TV hides an audio track it can't play (DTS, TrueHD), the other tracks keep the names from the file instead of a generic one.
- **Player:** UP at the top of the controls hides them (#305); the Seekr time and preview follow the hold.
- **Live TV:** a channel queued while zapping keeps its add-on (#283).
- **Trakt:** the watchlist is read page by page, up to 400 items.
- **Trakt/Simkl sign-in** no longer freezes the screen while sending credentials.
- Saved: section label follows the open card. Glass outline follows the poster radius. Back from the add-on and plugin lists returns to the row you came from.
- **Sync** summary is translated on the spot, and the Continue Watching source "Both" is now "All sources" (#312). With your own Seekr key there is no 50-lookup cap and no counter.
- A username and password inside an add-on URL no longer show up in the log.
- The log is sent once per session, at most 64 KB, and records the settings you change.
- **Posters with long image URLs** (over ~500 characters, such as rating-poster services) are no longer cut on the backdrop, the title page, Saved and the account library, so they load; a background or logo URL too long to keep is dropped with a log line instead of loading as a broken link (#361).

## Notes

| Platform | File |
| --- | --- |
| LG webOS 3+ | `space.nuvio.native.legacy_2.0.3_arm.ipk` |
| LG with more RAM | `space.nuvio.native.legacy_2.0.3_arm-highcache.ipk` |
| Samsung Tizen 4 / 5 / 5.5 | `Nuvio-2.0.3-NuvioTpk40.tpk` |
| Samsung Tizen 6 | `Nuvio-2.0.3-NuvioTpk60.tpk` |
| Samsung Tizen 6.5 / 7 | `Nuvio-2.0.3-NuvioTpk65.tpk` |
| Samsung Tizen 8 / 9 | `Nuvio-2.0.3-NuvioTpk.tpk` |
| Samsung web app, Tizen 5.5+ | `NuvioTV-2.0.3-tizen.wgt` |
| Android TV / Google TV, Android 7+ | `Nuvio-2.0.3-android.apk` |

If something breaks, send the log code from Settings › About and help.
