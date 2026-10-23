# Nuvio Legacy 2.0.9

Changes since `v2.0.8`, from local commit and branch snapshots. Integration does not confirm installation on a TV. English follows the owner’s current language override.


## Highlights

- **A richer pause panel**: After four seconds paused, see the episode synopsis, enabled ratings and rotating Trakt comments, with optional cast and local visibility settings. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #627, @I0-oX, `288115d6e`, `ef1b66a97`.

- **TorBox Airlock from the TV**: Keep or remove a source item from permanent Airlock storage directly in its files list, with refusal feedback and account-aware state. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #633, @I0-oX, `bcc63e3b1`, `7715cabc7`, `a01e8a134`, `ce6a7d1cd`.

- **Soundtracks on title pages**: Browse film and season soundtrack tracks, save albums to your profile Library, and open listening links on your phone using a QR code. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `ff262d549`, `e5cd72149`, `cef09beab`, `b74db6de6`, `c3dd1b806`, `4ef90be21`, `33e728d1b`.

## New

- **Profile settings and parental controls**: Separate TV and profile settings, protect administration with a PIN, copy or restore profile settings, and edit another profile with account actions guarded. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request; agente/205-205-ajperfil21, `0b5aadb03`, `5b6521fcf`, `69abb0294`, `6b8bb2d9b`, `470a5780c`, `914279f7f`, `2801ffa08`, `3e7864bf7`, `e37ca493e`, `bcbf9b13e`, `14c3ea923`, `f40ae2c10`, `e8259e3f2`.

- **Import your own fonts**: Import interface and subtitle fonts independently, preview the interface font, and retain selections during playback and backup. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #573; agente/205-pr573, `7f359731e`, `87dc223cc`, `857e3c215`, `8002ef61f`, `eb86643a5`, `0a9382efd`.

- **Player format display choices**: Choose pills, text, marks, badges or no format row, place it under the title or in the corner, and choose whether to show the audio codec alongside its language. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #669, @I0-oX, `e76811f15`.

- **Guest stars in this episode**: Open the player episode panel to see guest stars supplied by TMDB. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `26f616d6d`, `5f0913815`.

## Fixes

- **More complete skip markers**: Fill missing intro and credits marker types from IntroDB so available skip actions remain accessible. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #615; agente/205-pr615, `2cb47b044`.

- **Supported add-on source searches**: Resolve pending manifests before searching sources so requests use the add-on's supported resources. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-stream-antes-manifesto`, `ea677f958`, `179148c79`, merge `cd006c785`.

- **Continuous Android live playback**: Reconnect live transport streams before extractor EOF and preserve playback timestamps across reconnects and split PES packets. Platforms: Android. Refs: owner request, `agente/205-live-eof-android`, `fa65837dd`, `d7542ec37`, `4bfa8c935`, merge `fbc54d787`.

- **Account polling recovery**: Keep the account bearer identity stable and back off unauthorized polling requests. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `2a336785b`, merge `2c94a7b37`.

- **Identifiable Wikimedia requests**: Send the project's public contact when requesting Wikimedia content. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `be1a41bf8`.

- **Primary Samsung subtitles**: Selected primary subtitle cues render during native fallback instead of remaining invisible. Platforms: Samsung .tpk. Refs: #671, #665, `19533c9ad`.

- **Manual source selection everywhere**: Every playback request waits for your source choice when manual selection is enabled, and exact source picks are preserved. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #667, `3b0699466`, `e6544a115`.

- **Complete collection showcases**: Reach every collection catalog while scrolling, within the bounded cache. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #661, `2720a44b5`.

- **Stable Cover Flow loading**: Keep the visible catalog presentation stable as additional pages arrive. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #662, `a0ae67373`.

- **Hold to scroll the TV guide**: Held arrow keys continue through channels without unexpectedly opening the category sidebar. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #668, `133cccd0b`.

- **Live TV add-on playback**: Live catalogs and typed detail playback work again, and expanded channel aliases are matched before stream-prefix filtering. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #646, `63dbdadb2`, `3f503f285`.

- **Correct next-episode artwork**: Next Up artwork matches the episode actually offered. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `7954f6e5b`.

- **Custom add-on video identities**: Keep custom video IDs and route stream requests using the add-on manifest. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `465bfa48c`.

- **Collection filters and card styles**: Preserve web-snapshot tab filters, enforce media types and Top 10 limits, and respect round cards in collections and showcases. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `54ce9fcf9`, `4fd572bce`, `71cd70dcd`.

- **Profile animations and clean Light backgrounds**: GIF avatars restart whenever focused, and Light backgrounds avoid coarse film grain. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #660, #657, `58a94f01d`, `696409f3a`.

- **Localized post-play hints**: Post-play hints stay inside their translated islands. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `d9275be11`.

- **Consistent 12-hour time**: End times, Seekr local times and friend-activity timestamps follow your clock-format setting. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `bedce6987`.

- **More reliable scraper searches**: Avoid search starvation and report incomplete searches accurately. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `be73af022`.

- **Safer proxy playback and buffer fallback**: Playback fixes protect request headers after HTTPS downgrades, restore native buffering after proxy fallback, prevent concurrent clients from restarting the active stream and unblock near-forward native seeks when the origin stalls. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `agente/205-ur673`, `d365dd646`.

- **Artwork recovery on low-memory TVs**: Request smaller artwork and recover from memory pressure without leaving visible images grey. Platforms: webOS. Refs: #676, `agente/205-676-arte`, `e3427954b`.

- **Stable Home row focus**: Keep the selected item identity current after navigation so later row refreshes preserve focus. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #677, `agente/205-677-foco`, `e94610c5b`.

- **Personalized add-on catalogs**: Apply required manifest defaults when requesting catalogs such as AI for you. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #678, `agente/205-678-aiforyou`, `99d7014da`.

- **Library anime categories and filter focus**: Place Simkl anime movies under Movies and retain focus on the media-type control after switching categories. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-lib-anime-foco`, `bf3da4bc2`, `6e9f8233c`.

- **Faster confirmed live streams**: Bound live startup probes and open confirmed transport streams directly. Platforms: Samsung .tpk. Refs: owner request, `440074537`.

## Performance

- **Responsive artwork uploads on TCL**: Load cached artwork on a shared graphics worker and defer unavailable, contended or budget-blocked uploads to keep navigation responsive. Platforms: Android. Refs: owner request, `agente/205-lento-tcl`, `9756ff9e9`, `44014cf78`, `d8b60bef5`, `38e369735`, merge `90e5b321a`.

- **Bounded source-sheet work**: Limit source-sheet frame work to visible rows to keep navigation responsive. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `d28ff9e77`.

- **Protected native HTTP read-ahead**: Native playback reads ahead in RAM with memory safeguards, responsive replacement ranges and seek handling, and contiguous buffered progress on the standard rail. Platforms: webOS, Android, Samsung .tpk. Refs: PR #673, `e19571da2`, `7547e3715`, `396c2a70e`, `5462e5933`, `f35f5a25a`, `d365dd646`, `0eb86c3be`.

## Community

- **Pause-panel contribution**: Enjoy the expanded pause experience contributed by @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #627, `288115d6e`.

- **Airlock contribution**: Manage TorBox permanent storage with the contribution from @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #633, `bcc63e3b1`.

- **Player-format contribution**: Customize player format and audio-language presentation with the contribution from @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #669, `e76811f15`.

## Internal

- **Font and upload regression evidence**: Cover imported fonts, subtitle rendering, marker fixtures and graphics upload limits without changing release behavior. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `c2e7d0ef9`, `bca33e5b1`, `42cfd5215`, `be613c39e`, `00d81ca89`, `1faca80c1`, `2ca240e08`, `361fb458d`, `b92af68f9`, `abeb4ad04`, `8c25bc1e0`.

- **Scoped review validation**: Record collection and scraper review validation without expanding the approved fix scope. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `1b1cb55c2`, `969b0614f`.

- **Release tracking**: Keep issue states, review follow-ups and release coverage aligned with the local integration snapshot. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `edd72dff1`, `05d582211`, `1137c9f39`, `9cc5a41aa`, `103c96e62`.

- **Soundtrack regression coverage**: Verify soundtrack panel sizing and paging through remote keys. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `dd8c54a6a`, `2eb4fecc0`.

## Pending (not yet integrated)


- **Richer soundtrack catalog data**: Add Apple catalog artwork and track preview metadata with fallback lookup; the redesigned app UI and preview playback still await implementation and approval. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-musica-visual`, `3c65dda20`.
