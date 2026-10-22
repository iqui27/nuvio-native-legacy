# Nuvio Legacy 2.0.2

A visual refresh, a lot more control over playback, and faster starts.

## Added

- **New Home hero.** Accent-colored button the same size as the title page buttons, page dots that follow your accent, and a softer gradient over the trailer.
- **Context menu redone.** The poster turns into an info card with the menu beside it. On large cards, and on 4:3 Featured cards, the info sits over the image. The menu also works in lists, and the Social panel opens inline.
- **Title page "..." button** with Trailer, Explore and Change artwork.
- **Season progress chart** on series pages, with how far your friends are.
- **Settings previews** are now generated and animated, in every scene.
- **Social panel header.** Tabs beside the title, an Edit sheet per profile, and an Agenda tab that lists upcoming episodes by date, with rows like the Agenda screen. The panel animations follow the clock island.
- **Watch history target per profile**, with "Send history to the Nuvio account", and a **Social features** switch to turn Social off.
- **What's New 2.0.2** card.
- **Friends tab simplified.** A single "Add people" button and your linked accounts in one row.
- **Row and title spacing** on Home, in Settings.
- **Default aspect ratio** applied when playback starts (#300).
- **Hide the parental guide** in the player.
- **Auto-play rules like official Nuvio:** allowed add-ons and plugins, regex filter, and an instant-wait option.
- **Also in Continue watching:** keep a held title in the row as well.
- **Continue on profile picker** (#303).
- **Search:** a switch to turn Cinemeta results off (#311).
- **Interface resolution: Automatic.** The 4K interface is only used when the TV can handle it. Everyone is moved to Automatic once.
- **DTS on LG** can now be converted to stereo or to 5.1.
- **Dolby Vision in MKV on LG only** (experimental, off by default).
- **Player:** audio codec shown (#293), forced subtitles (#287), resolution in the sources list, Arabic subtitles (#273).
- **ASS subtitles on Samsung .tpk** are now drawn by libass, with styles and positioning, on all four packages.
- **Auto-play add-on order:** off, tie-break or strict.
- **Faster start options:** play while verifying, warm connections and a parallel check of up to 3 ready debrid sources (on by default), and prepare the source when a title opens (off by default).
- **Add-on text mode** keeps the add-on's own lines (up to 5), with icons for common emojis and trimmed separators.
- **Samsung** warns when the TV can't play DTS or TrueHD, and prefers Dolby sources.
- **LG:** add-on live channels work (#283). CH+ opens Salvos, like on the other platforms.
- The account sync summary and the Trakt/Simkl link screen are translated.

## Fixed

- **Faster stream start without lowering quality.** If a source gives no signal, the next one is tried after 15 s. Slow or silent add-ons no longer hold up auto-play, and the clock island tells you what it is waiting for.
- **P2P files over 2 GB** on Samsung and LG (#297).
- **Samsung:** embedded subtitles in MKV files over 2 GB (#269).
- **Android 11:** a black screen at launch now shows a rescue screen, and the log is sent even without login (#266).
- **Android TV:** the app froze when opening a second movie.
- **LG:** DTS audio hidden by the TV is recovered (#301).
- **Auto-play skips sources that aren't video.** They show dimmed as "Not a video" in the list.
- **Samsung:** embedded SRT subtitles are drawn by the app. Less log noise.
- **LG:** subtitles engine only warms up when a track needs it.
- **Skip credits** button hides itself after 10 s.
- **Live TV** reconnects when a channel stops playing (#302).
- **Samsung Tizen 4 with Mali-400:** the "Light" effects level now drops to minimal (#286).
- **Home** drops rows of removed or disabled add-ons right away, and they no longer take Home slots (#319).
- **MStar Android boxes:** no more flicker from the video surface switching, and the remote stays bound to the app (#318).
- **LG:** startup breadcrumbs and a crash report even before the first frame (#317).
- Smoother and lighter on slower TVs.
- Home trailer fade matches the still art (#290), plus fixes for #294, #295 and #289.
- More audio and video details in the logs on TCL.

## Notes

| Platform | File |
| --- | --- |
| LG webOS 3+ | `space.nuvio.native.legacy_2.0.2_arm.ipk` |
| LG with more RAM | `space.nuvio.native.legacy_2.0.2_arm-highcache.ipk` |
| Samsung Tizen 4 / 5 / 5.5 | `Nuvio-2.0.2-NuvioTpk40.tpk` |
| Samsung Tizen 6 | `Nuvio-2.0.2-NuvioTpk60.tpk` |
| Samsung Tizen 6.5 / 7 | `Nuvio-2.0.2-NuvioTpk65.tpk` |
| Samsung Tizen 8 / 9 | `Nuvio-2.0.2-NuvioTpk.tpk` |
| Samsung web app, Tizen 5.5+ | `NuvioTV-2.0.2-tizen.wgt` |
| Android TV / Google TV, Android 7+ | `Nuvio-2.0.2-android.apk` |

Tested on an LG C9 and a TCL Android TV. Dolby Vision in MKV was only tested on a C9 (webOS 4.10). The Samsung builds were not run on a Samsung TV. If something breaks, send the log code from Settings › About and help.
