# Nuvio Legacy 2.0.5

The update after 2.0.4: a new opening, a full subtitle list and a long list of fixes. Community server: **https://discord.gg/9NWr6SHyzJ**

## Added

- **Cinema opening.** The cinema-style opening screen is now the default. The detailed card stays in Settings.
- **Marked as watched only after real playback.** A trailer no longer marks the title as watched.
- **Skip intro.** New options to skip the intro, the recap and the credits on their own.
- **Full subtitle list.** Subtitles grouped by language, with the automatic sync row.
- **Second subtitle.** Holding OK opens more options and turns the second subtitle off. Size up to 300%.
- **Logos and details.** Title logos and information appear without waiting for the full detail.
- **Profiles.** Continue Watching under each avatar, and quick profile switching.
- **Add-ons.** A button to sync the account's add-ons. The TV only receives, it never sends.
- **App icon.** New app icon, with the LEGACY tag.
- **Image memory.** Settings show the image memory limit of your TV.
- **Samsung .tpk.** The native player no longer freezes the interface, and the aspect button changes the picture.
- **LG webOS.** Its own Homebrew Channel repository, to install and update.
- **Source details** (#355): the info button on a source opens a card with the add-on, the label, the full description and the file name. The link itself is never shown.
- **Title page, cleaner.** The synopsis stops at 3 lines, the details sit on one line (genres, year, runtime and rating, with year and runtime in bold), and the "next episode" badge lines up with the text.
- **Resume button.** Shows how much you have watched as a small bar and the time left (like "1h 51min"), instead of a separate "resume available" line.
- **New font.** JetBrains Sans, the font of Nuvio Desktop, is now an option in Settings > Interface font.
- **Library** (#352): a watched badge on the poster and on the row, and sorting by watched / unwatched.

## Fixed

- **Automatic subtitle sync swaps the subtitle at most once per episode**, and goes back to your original when the candidate has under half of its lines.
- **LG: automatic subtitle sync now runs.** Reading the reference used to give up because of the TV's short buffer (it stopped after 45 seconds every time).
- **The manual sync button shows what it is doing** on the clock island, with or without automatic sync; a refused press is announced.
- **Subtitle blocks with invalid times are ignored** instead of marking the whole subtitle as incomplete, and an unfinished download no longer changes the subtitle on screen.
- Automatic subtitle sync follows the redirect of a link and sends the add-on's own headers, so it can read the reference for more sources.
- Subtitle matching now compares against the file actually playing, fetching a subtitle no longer holds the video for up to 4 seconds, and the first line no longer shows a wrong "failed" message.
- A subtitle you picked by hand survives into the next episode (#383); forced subtitles and unknown types keep their behaviour, and picking the active subtitle from outside the recommendations no longer lands on "Off".
- Embedded text subtitles are drawn by the app on LG and Samsung (#421); on LG their size matches the preview.
- The Serbo-Croatian code matches Croatian, Serbian and Bosnian (#429), and the exact language wins when a language is picked automatically.
- Subtitle sizes are the same everywhere and go up to 300% (#439, #440), and Arabic bold matches the regular size.
- **Marked as watched requires real playback** (#205): a trailer, a paused full-screen video or closing the player early no longer counts, your real resume point is kept, and cancelling "Next" keeps your place even after rewinding.
- A resumed episode only counts the part actually played, including the post-credits tail (#205, #100); choosing to resume clears a title that was stuck as paused.
- Post-credits: the next-episode card waits for the scene, you can go to the next episode by hand while it plays, and the watched mark reaches every row and survives a refresh.
- When the player refuses a source, playback falls back to the next one instead of failing (#364); the "Try next source" button responds to touch again, and switching source no longer erases a message on screen.
- The source automatic playback used is remembered when you open the next episode by hand (#310).
- A source is only marked unavailable after it really failed; those marks are cleared when the title plays, failed opens are remembered for 6 hours so failover follows the list order, and a source that fails when you pick it by hand is not offered again right away.
- Torrents the debrid has not cached are marked and sorted last (#402).
- Starting a video by hand no longer waits forever when it fails, and the automatic jump to the next source respects your "another source if it fails" setting (off means only the warning shows).
- A cached runtime no longer gets applied to a later episode that happens to have the same number of episodes, and the player warns when a source's length does not match the title.
- Rewinding no longer gets stuck after the video ends or while credits are being skipped.
- The on-screen controls stay up while you are still using them.
- **Android Dolby Vision: aspect-ratio modes work again**, a slow 4K / Dolby Vision source is no longer dropped right after the first frame, and resizing the window no longer restarts the picture.
- The interface no longer flashes white while an HDR or Dolby Vision video plays (#205).
- Samsung aspect ratio: the picture stays where it belongs on Tizen 4 (#426, #341), an off-screen crop is refused instead of pushing the image away, and a refused or queued change gives the sound back and stays in its own video.
- Samsung Live TV: channels that need the add-on's own headers now play, a channel reopens at once when its stream ends (#302), and reconnect loops are bounded.
- Letterboxing is kept for plain full-screen requests.
- Continue Watching keeps its saved slot while catalogs load (#392); "Trakt only" without a Trakt link falls back to all sources; and the newest progress wins when copies disagree.
- A new add-on catalog that no longer fits on Home goes to the queue instead of being switched off (#358), and a row switch now takes effect instead of flipping back.
- Home no longer shows a catalog that no add-on provides when the row list is full; your explicit choice is kept (#204).
- Row add/hide no longer fights the account's list: adding wins over the account's hidden rows, and hiding no longer gets stuck.
- Account sync keeps your local order until you reorder on the TV, imports custom titles, and respects the account's add-on order (#204).
- Metadata loads again when the main TMDB address is blocked or times out (#416, #419), and posters with very long addresses load again (#433).
- The next-episode countdown only starts when the credits start is a real marker (never the 2-minute estimate), 4 seconds after the card shows (#331).
- Auto-skip of credits only fires with a real start and end marker (#329, #331) and rewinding never skips them again; intro markers also cover anime opened from IMDb/TMDB, in any season (#430).
- **Depth** now controls the sheen you actually see on the focused card, with a single intensity slider instead of the old per-card switches.
- Settings: the image memory choice is recorded only after your settings really save, explicit choices survive the move from the old highcache package, and clicking a limit the TV does not support does nothing.
- The TV guide keeps its normal layout on TVs; the one-column layout now only applies to mobile.
- The comments picker stays on the episode you confirmed and reopens on its season after you browse others.
- Holding OK only fires when you really hold it, not on the frame after a long pause (#387).
- Touch controls no longer react on TVs: mobile buttons, touch scrolling and editing stay inert in TV mode.
- Renewing your session no longer drops consent or mixes up the active account and profile.
- The title page shows one trailer card again (#417), and the screensaver waits while a trailer plays (#422).
- The speed test now measures sources that stream as raw data (#203).
- The P2P space limit has a hard, choosable cap with a clear message (#334), and videos larger than the cap keep playing instead of stopping in the middle.
- Player controls, live TV, format badges and the next-episode card line up with the clock island; island panels show the title, and badges no longer animate on every key press.

## Known issues

- On some Android TVs a Dolby Vision episode can occasionally start dark until the aspect ratio is changed. Still under investigation; a log code sent right after it happens helps.
- Samsung: automatic subtitle sync is not available yet; manual delay works.

## Notes

Samsung .tpk: this version changes the .tpk's host code again (interface freeze #412, aspect ratio #426), so **the .tpk has to be reinstalled**; the in-app update alone does not deliver these fixes.

2.0.5 is the last release that ships the highcache package: the image memory limit is a Settings option now, and a highcache install takes the normal package on the next update.

| Platform | File |
| --- | --- |
| LG webOS 3+ | `space.nuvio.native.legacy_2.0.5_arm.ipk` |
| LG with more RAM | `space.nuvio.native.legacy_2.0.5_arm-highcache.ipk` |
| Samsung Tizen 4 / 5 / 5.5 | `Nuvio-2.0.5-NuvioTpk40.tpk` |
| Samsung Tizen 6 | `Nuvio-2.0.5-NuvioTpk60.tpk` |
| Samsung Tizen 6.5 / 7 | `Nuvio-2.0.5-NuvioTpk65.tpk` |
| Samsung Tizen 8 / 9 | `Nuvio-2.0.5-NuvioTpk.tpk` |
| Samsung web app, Tizen 5.5+ | `NuvioTV-2.0.5-tizen.wgt` |
| Android TV / Google TV, Android 7+ | `Nuvio-2.0.5-android.apk` |

If something breaks, send the log code from Settings › About and help.
