# Nuvio Legacy 2.0.4

A hotfix on top of 2.0.3, plus a few small additions. Community server: **https://discord.gg/9NWr6SHyzJ**

## Added

- **Discord server.** The "What's new" card and Support the project show a QR code to join: https://discord.gg/9NWr6SHyzJ
- **Source order** (#400): Settings › Playback › More options lets you keep the sources by quality or exactly as the add-on sent them (for AIOStreams and similar, which already sort).
- **Automatic subtitle sync can be turned off** (Settings › Subtitles, next to "Sync by audio"). It stays on by default.
- **"Unmark from here on"** in the episode menu, next to "Unmark up to here". Both range actions now cover the whole show, not only the season on screen.
- **Next episode opens faster:** its sources are fetched 10 seconds before the up-next card appears.
- **LG: Dolby Vision profile 5 in MP4** plays as Dolby Vision.
- "Use the primary profile's add-ons" is now off by default for new profiles; profiles that already had it keep their choice.

## Fixed

- **"The source didn't answer in time" on sources that work** (#409). Auto-play now skips 4K sources the TV's decoder cannot play and waits for the previous player to be released before opening the next one.
- **Android TV and Samsung .tpk: slow 4K sources are no longer dropped while they are opening.** A source that had already answered (tracks known) but had not shown its first frame within 15 seconds was treated as dead, so auto-play jumped to the next one and could take 40 seconds to start. Such a source now gets the full 30 seconds; a source that sends nothing is still skipped at 15.
- **Samsung .tpk: the interface no longer stalls while a video opens or closes** (#412). This changes the app host, so **the .tpk has to be reinstalled**; the in-app update alone does not deliver this fix.
- **Samsung Tizen 4 / 5: no more "Not Available" toast** on Play/Pause and other media keys (#411).
- **Glass outline, Depth, edge glow and Sheen respond to their settings again** (#410). The automatic graphics level now starts from full effects and only steps down when the TV measurably cannot keep up.
- **Library showed "129 saved" but only a few titles** when "Hide unreleased" was on: titles without a year were treated as unreleased. Only titles with a known future date are hidden now.
- **Spotlight:** your own titles (Continue Watching, saved, in progress) come first among equal matches, the last row is never drawn clipped, OK opens the title you see even if the list refreshed, and artwork no longer mixes a movie with a series of the same name.
- **Series pages are smooth again** when opened from Continue Watching or Spotlight: the progress chart was being rebuilt many times per frame.
- **Automatic subtitle sync no longer swaps the subtitle you chose** when the video's own reference track is too short to compare (a forced track with a few lines), and no longer says "Couldn't download the subtitle" for a subtitle that downloaded and is on screen.
- Subtitles-only add-ons (OpenSubtitles and similar) no longer appear as "did not respond" in the source list.
- **TV guide: search no longer crashes** when you press "done" on the keyboard and a channel has more than 24 programmes in the next 6 hours (#344). Searching now also looks past the first 24 programmes of such a channel.
- **Season tabs match the episode list** (#372, #328). When a metadata add-on supplied a longer episode list than the Nuvio catalog (anime such as The Apothecary Diaries), its episodes were loaded but the page kept one season tab, so only season 1 was visible. The tabs now come from the list that is actually shown, and reopening the title while its cast and artwork are still loading no longer brings the old tabs back.
- 2017 LG TVs (webOS 3): the app no longer closes when opening a video for the second time in a session (the DTS fallback check loaded and unloaded LG's media player library on every video; it now loads once and stays).
- **Series no longer show the same episode on every card.** When a stream add-on answered the episode request with its list of torrent files (thousands of entries repeating the same few episodes), that list replaced the real one: every card read "Episode 1" with "1200 of 1200 watched" (2.0.2), or the series was left with only a handful of episodes (2.0.3). Add-on lists are now compared by distinct episodes, and a file list is only used when it really knows more episodes than the catalog (the catalog's episode names are kept where both have them). This also covers series opened from that add-on's own catalog.
- 2017 LG TVs (webOS 3.9) are no longer treated as webOS 4: the version now comes from the TV's own `webos_release` (the same one the log's `[tv]` line shows), so Dolby Vision in MKV, which needs webOS 4, stays off on them.
- Switching profile and coming back no longer rearranges the Home rows (collections, catalogs, Continue Watching): the other profile's addon catalogs are no longer registered into the profile you return to (#392).
- Sources labelled only "FHD" or "Full HD" (no "1080" in the name, as some AIOStreams formats write it) are now grouped under 1080p instead of "Other" (#402).
- **Custom poster URL template is saved again** (#390). Long templates (up to 400 characters) and API keys with uppercase letters are kept as typed; before, anything past 299 characters was cut and the key was lowercased, so the template was rejected without saving. A template whose finished address could not fit is refused when you save it, with a message.
- Subtitle and audio languages set to "From account" now apply even when this TV has protected local settings, and switching profile no longer carries the previous profile's languages over (#378).
- Android boxes with Amlogic chips (Mecool KM7 SE and similar): HDR videos no longer recreate the video surface at the start. On low-memory boxes that recreation could crash the app or freeze the box when a 4K HDR episode started.
- 2017 LG TVs: a read error on the secondary version file no longer throws away the webOS version already read from the main one.
- Custom poster URL template that is too long once filled in now says so, instead of the generic "invalid template" message.

## Known issues

- On some Android TVs a Dolby Vision episode can occasionally start dark until the aspect ratio is changed. Still under investigation; a log code sent right after it happens helps.

## Notes

Samsung .tpk: install this version by hand (see the #412 item above).

| Platform | File |
| --- | --- |
| LG webOS 3+ | `space.nuvio.native.legacy_2.0.4_arm.ipk` |
| LG with more RAM | `space.nuvio.native.legacy_2.0.4_arm-highcache.ipk` |
| Samsung Tizen 4 / 5 / 5.5 | `Nuvio-2.0.4-NuvioTpk40.tpk` |
| Samsung Tizen 6 | `Nuvio-2.0.4-NuvioTpk60.tpk` |
| Samsung Tizen 6.5 / 7 | `Nuvio-2.0.4-NuvioTpk65.tpk` |
| Samsung Tizen 8 / 9 | `Nuvio-2.0.4-NuvioTpk.tpk` |
| Samsung web app, Tizen 5.5+ | `NuvioTV-2.0.4-tizen.wgt` |
| Android TV / Google TV, Android 7+ | `Nuvio-2.0.4-android.apk` |

If something breaks, send the log code from Settings › About and help.
