# Nuvio Legacy 2.0.9

Changes since `v2.0.8`, from local commit and branch snapshots. Integration does not confirm installation on a TV. English follows the owner’s current language override.

Tracking refreshed against the supplied snapshot through `c14d11f36`; all listed non-merge commits remain covered. Android build and installation succeeded at `cb7ece972`; C9 installation failed, leaving the installed TV revision unconfirmed. See the keeper state report for subsequent merges.


## Highlights

- **Personal title and episode ratings**: Rate titles and episodes, retain ratings per profile, reopen the star editor and use quick tags with undo; send supported ratings to Trakt and Simkl. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-205-avaliar21`, `3f70510ea`, `892f61354`, `ecc7119c8`, `303d852c0`, `6d6ca3c2d`, `22c77e099`.

- **A richer pause panel**: After four seconds paused, see the episode synopsis, enabled ratings and rotating Trakt comments, with optional cast and local visibility settings. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #627, @I0-oX, `288115d6e`, `ef1b66a97`.

- **TorBox Airlock from the TV**: Keep or remove a source item from permanent Airlock storage directly in its files list, with refusal feedback and account-aware state. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #633, @I0-oX, `bcc63e3b1`, `7715cabc7`, `a01e8a134`, `ce6a7d1cd`.

- **Soundtracks on title pages**: Browse film and season soundtrack tracks, save albums to your profile Library, and open listening links on your phone using a QR code. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-musica-visual`, `agente/205-musica-png`, `ff262d549`, `e5cd72149`, `cef09beab`, `b74db6de6`, `c3dd1b806`, `4ef90be21`, `33e728d1b`, `3c65dda20`, `4c54f0b8a`, `204cd9980`, `9c2122094`, `239e08b51`, merge `af159f336`, merge `92aaee057`.

## New

- **Readable Arabic interface fonts**: Use subsetted Noto Sans Arabic with real bold text and corrected Settings paragraph and label alignment. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `d9a10596a`, `d11a8ef63`, `b5a2b3358`.

- **Smoother TV gradients**: Choose gradient smoothing to reduce visible banding in video overlays, with translated controls. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `007b5009e`, `1770f7bf4`.

- **Profile settings and parental controls**: Separate TV and profile settings, protect administration with a PIN, copy or restore profile settings, and edit another profile with account actions guarded. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request; agente/205-205-ajperfil21, `0b5aadb03`, `5b6521fcf`, `69abb0294`, `6b8bb2d9b`, `470a5780c`, `914279f7f`, `2801ffa08`, `3e7864bf7`, `e37ca493e`, `bcbf9b13e`, `14c3ea923`, `f40ae2c10`, `e8259e3f2`.

- **Import your own fonts**: Import interface and subtitle fonts independently, preview the interface font, and retain selections during playback and backup. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #573; agente/205-pr573, `7f359731e`, `87dc223cc`, `857e3c215`, `8002ef61f`, `eb86643a5`, `0a9382efd`.

- **Player format display choices**: Choose pills, text, marks, badges or no format row, place it under the title or in the corner, and choose whether to show the audio codec alongside its language. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #669, @I0-oX, `e76811f15`.

- **Guest stars in this episode**: Open the player episode panel to see guest stars supplied by TMDB. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `26f616d6d`, `5f0913815`.


- **Quick episode chips and saved polish**: Jump episodes with quick chips on a 3x2 paged grid, see a compact rating on saved cards and sweep fill, remote-style keycaps on hints, localized island copy, a Remove action on the panel card, and glass-outline focus accents. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-avaliar-ilha`, `agente/205-porta-perdidos`, `755a4aaf0`, `dc2f11f32`, `601ceec5b`, `f933eaa3d`, `22c77e099`.

- **See all in the Library row**: Open the full Library from a See all card at the end of the Home row. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #672, `d316b4dbd`.

## Fixes

- **Skip uncached warning clips safely**: Automatic playback stops provider warning clips and tries eligible alternatives, while manually selected or explicitly accepted short sources remain playable. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-foracache`, `185f12732`, `4ee2b095d`, merge `287694ce4`.

- **Long signed source URLs remain playable**: Preserve complete signed media URLs up to 8191 bytes across playback and saved sources, release their storage with source lists, and skip oversized entries without discarding playable alternatives. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-urllonga`, `26ddf22d7`, `01d39cbb2`, merge `0152ed9bf`.

- **Focus accents recover consistently**: Restore accent focus independently of the Glass setting. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-amigosacento`, `790abb451`, merge `93c397353`.

- **Reach the clock with Up**: Navigate from Home rows to the clock and resume island using Up. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #547, @ghbarker, merge `f895747c5`.

- **Consistent alternate episode numbering**: Keep watched state, playback and scrobbling tied to the canonical episode while preserving the chosen episode order and retry identity. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #576, `e2d98f5f0`, `aa4095866`, merge `940288b03`.

- **Subtitle filename fallbacks**: Recover subtitle requests when an add-on supplies an alternative filename. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #606, `c9ae2eb50`, merge `610487f8a`.

- **Library navigation to the top menu**: Press Up to reach the Library top menu without losing navigation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `add7912d2`.

- **Accurate upscaling availability**: See whether upscaling is available in the current session and why it is disabled. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `85b0ac386`, merge `4355ba714`.

- **Retryable episode metadata**: Missing or invalid Cinemeta metadata remains retryable, while confirmed absence is cached for 24 hours to avoid repeated requests. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #356, `5058b9870`, `aab25a275`, `a6b791713`, `a4fec2800`, `19ef559f3`.

- **Samsung startup pause recovery**: Recover from an unexpected pause during startup after the firmware finishes hiding the windows, with one bounded retry before interaction. Platforms: Samsung .tpk. Refs: #684, `3f27a741a`, `133cbf6c2`, merge `7102f722e`; physical TV confirmation pending.

- **Buffered live TV recovery**: Retry buffered webOS live sources through the transport-stream proxy when native playback fails. Platforms: webOS. Refs: #625, `1b84d0a2f`, merge `9a7d40f3e`.

- **Fewer false offline states**: Require failures on different hosts before Android declares the connection unavailable. Platforms: Android. Refs: owner request, `a0efe5bee`, merge `3a1dbb575`.

- **Episode progress survives source changes**: Switch playback sources without losing episode identity or watched state. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #570, `ee42d28cc`, `63ff7c56f`, merge `37fdc3ea0`.

- **Older Samsung package compatibility**: Rebuild the rate-limited artwork sources correctly so the Tizen 4/5 package can link. Platforms: Samsung .tpk. Refs: PR #688, `00192c3fd`.

- **Artwork restored on Samsung**: Use the active graphics context so posters and other artwork can upload instead of remaining blank. Platforms: Samsung .tpk. Refs: PR #685, `089f694b9`.

- **Selected Samsung subtitles survive seeks**: Keep your chosen subtitle language when an intro skip or seek overlaps startup track selection. Platforms: Samsung .tpk. Refs: PR #686, `fa3c6a936`.

- **Correct series identity after a title-ID collision**: Episode evidence keeps a series from appearing as an unrelated film and repairs affected Continue Watching and statistics entries without changing unrelated titles. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #524, PR #654, `42b9719c7`, `63ff7c56f`.

- **Consistent collection timeline spacing**: Collection timeline cells use the same height for layout and drawing, keeping rows aligned. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `3e00f1e4b`, merge `9b5511cda`.

- **Keyboard crash recovery**: If the webOS system keyboard crashes or fails to appear on webOS 26, input falls back to the app keyboard so you can continue searching and editing fields. Platforms: webOS. Refs: #642, `831cfca4a`, `749f6a0e5`, `481f890b0`.

- **Next episode focus and playback exit**: Series focus follows the next episode, and playback exits at the end when the post-play prompt has already been dismissed. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #674, #675, `9e3cf67d8`.

- **Add-on source status translated**: The "Searching add-on sources…" and "not responding" lines in the sources sheet now follow the interface language instead of always showing Portuguese. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `f49598336`.

- **Broader anime source matching**: Translate anime IDs through all ARM endpoints and query each declared namespace in parallel while preserving explicit specials, source ID spaces and safe AIOStreams deduplication. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `dd70f889e`, `1511b615c`, `08673b9ea`, `af48dee49`, `680ab96e7`, `fbd7c059b`.

- **Compatible poster templates**: Accept Nuvio poster templates that specify alternative title identifiers. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `8f99c7b43`.

- **Kaptain discovery filters**: Retain numeric TMDB discover filter values when loading Kaptain catalogs. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `4788e53fb`.

- **Upcoming cards after Continue Watching**: Keep split upcoming cards visible even when the Continue Watching row is full. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `b83185b9f`.

- **More complete skip markers**: Fill missing intro and credits marker types from IntroDB so available skip actions remain accessible. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: PR #615; agente/205-pr615, `2cb47b044`.

- **Supported add-on source searches**: Resolve pending manifests before searching sources so requests use the add-on's supported resources. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-stream-antes-manifesto`, `ea677f958`, `179148c79`, merge `cd006c785`.

- **Partial add-on tabs and sources while auto waits**: See per-add-on search state in the sources sheet and open Sources while the automatic choice still waits on slow add-ons; plugin tabs key by scraper id and refresh without sources, ending the duplicate tab per plugin that delivered sources. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `369b542dc`, `e1d363bed`, merges `4c6ca8198`, `f0794be5a`.

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

- **More sources when manifests are unclear**: Treat unreadable manifest resources as presumed capabilities and keep optimistic stream queries unless the add-on is live-TV-only. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `bf129e9af`, `606a892e8`, `bf6757c3a`.


- **Safer Dolby Vision picking**: Keep MP4 Dolby Vision profile 5 below same-band HDR10 so incompatible files lose the tie. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `d4f1dca81`.

- **Header-bound add-on streams on Tizen**: Relay through the local server the add-on sources that need headers the TV player cannot send, so they play instead of timing out. Platforms: Samsung .tpk. Refs: #648, `f7615ebea`.

- **Trailer sound help translated**: The rewritten trailer sound help follows the interface language. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #509, #653, `808fadff2`.

- **Collection showcase scrolling on Samsung**: Register pointer hit targets on showcase rows so hover and scroll work in TV pointer mode. Platforms: Samsung .tpk. Refs: #658, `a47aec93d`.

## Performance

- **Bounded source parsing**: Large source responses avoid repeated scans across unrelated stream objects, reducing source-list parsing delays. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-streamparse-quad`, `e8883d07d`; merge `31b2a6cb9`.

- **More reliable soundtrack lookup**: When iTunes throttles requests, fall back to Deezer (with 30-second previews) and MusicBrainz instead of returning empty, and stop caching a miss too early. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `2823b3c7f`.

- **Responsive artwork uploads on TCL**: Subsample artwork and bound texture pipeline memory, defer contended or budget-blocked uploads, and remove shared-context Android uploads to reduce memory pressure and navigation stalls. Row residency and shared artwork budgets are enforced and restored on Android. Platforms: Android. Refs: owner request, `agente/205-lento-tcl`, `9756ff9e9`, `44014cf78`, `d8b60bef5`, `38e369735`, `211efce22`, `a60aa5f86`, `e23561f55`, `483ff3348`, `a81e27c2c`, `2c3854211`, `395189bf0`, `b6ba4c9ff`, merge `90e5b321a`.

- **Bounded source-sheet work**: Limit source-sheet frame work to visible rows to keep navigation responsive. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `d28ff9e77`.

- **Protected native HTTP read-ahead**: Native playback reads ahead in RAM with memory safeguards, responsive replacement ranges and seek handling, and contiguous buffered progress on the standard rail. Platforms: webOS, Android, Samsung .tpk. Refs: PR #673, `e19571da2`, `7547e3715`, `396c2a70e`, `5462e5933`, `f35f5a25a`, `d365dd646`, `0eb86c3be`.

## Community

- **Navigation contribution from @ghbarker**: Reach the clock and resume island through the contributed remote navigation changes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @ghbarker, PR #547, merge `f895747c5`.

- **Samsung playback and artwork contributions**: Keep subtitle selection across seeks and restore artwork and older-TV package compatibility with contributions from @KeijoMika. Platforms: Samsung .tpk. Refs: @KeijoMika, PR #685, PR #686, PR #688, `089f694b9`, `fa3c6a936`, `00192c3fd`.

- **Series identity contribution**: Enjoy corrected series metadata from @Vidhin05, with integration follow-up limiting repairs to episode evidence. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @Vidhin05, PR #654, `42b9719c7`, `63ff7c56f`.

- **Pause-panel contribution**: Enjoy the expanded pause experience contributed by @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #627, `288115d6e`.

- **Airlock contribution**: Manage TorBox permanent storage with the contribution from @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #633, `bcc63e3b1`.

- **Player-format contribution**: Customize player format and audio-language presentation with the contribution from @I0-oX. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: @I0-oX, PR #669, `e76811f15`.

## Internal

- **Latest tracking snapshot**: Keep issue tracking aligned with the integration snapshot. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `c14d11f36`, `97c62989a`, `5404c634b`, `f6a6c3934`, `70e3ed2d3`, `279078d58`.

- **Episode and playback regression coverage**: Validate alternate numbering, topology replacement, retries, canonical scrobbles and isolated Home refresh fixtures. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `6537b2a36`, `be51fbfd0`, `587b8774b`, `f3ccc5b23`.

- **Accurate playback diagnostics**: Exclude suspended time from frame measurements and record each VOD stall once so diagnostic reports retain useful evidence. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: #681, `97314076a`, `3ce9e80d2`.

- **Samsung graphics and packaging regression checks**: Check active-context texture uploads and Tizen 4 rate-limit object selection. Platforms: Samsung .tpk. Refs: `1b805d013`, `7790e8b4f`.

- **Tracking refresh**: Additional refs: `6b86b60fe`, `cf141198f`, `be73960db`, `02bf3bf7e`, `ac3ca1e3a`, `2f7c7928c`, `f755a7aab`. Keep issue states and release coverage aligned with integration. Additional refs: `8fa52c05e`, `8ae0dc15f`. Additional ref: `95328a039`. Additional ref: `4465c41ed`. Additional ref: `43df4cdc9`. Additional ref: `f02196028`. Additional ref: `56dfcfdf6`. Additional ref: `19b0bf6b7`. Additional ref: `9780dd357`. Additional refs: `f07f0e64a`, `bf7c14f3b`, `bf890d535`. Additional refs: `683dc0b0c`. Additional refs: `7417183a8`, `21693a20c`. Additional refs: `83f87edfb`, `a99d9c0c9`. Additional refs: `dc8592948`, `31c4e71a3`, `76f31a538`, `5d9c3422e`, `fb80ad4e3`, `55cd43a20`. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `a4412a42a`, `c3ff78fe5`, `5eac48ad4`, `23121f28b`, `72cf607d6`, `0ce56918b`, `094b8f842`, `03932481c`, `6d0dee52d`, `584747b9b`.

- **Stream prefix regression snapshots**: Preserve integration stream-prefix fixtures for subsequent validation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `1b5766690`.

- **Rating and Arabic screenshot validation**: Exercise real title and episode rating flows, reject blank screenshots and capture Arabic Home and Settings views. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `55baaaad5`, `ca7571952`, `61987b4dc`.

- **Font and upload regression evidence**: Cover imported fonts, subtitle rendering, marker fixtures and graphics upload limits without changing release behavior. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `c2e7d0ef9`, `bca33e5b1`, `42cfd5215`, `be613c39e`, `00d81ca89`, `1faca80c1`, `2ca240e08`, `361fb458d`, `b92af68f9`, `abeb4ad04`, `8c25bc1e0`.

- **Scoped review validation**: Record collection and scraper review validation without expanding the approved fix scope. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `1b1cb55c2`, `969b0614f`.

- **Release tracking**: Keep issue states, review follow-ups and release coverage aligned with the local integration snapshot. Additional refs: `4d9d33519`, `618e7114c`. Additional refs: `40f0a2ecb`. Additional refs: `518e4afe2`, `676873f5e`, `68cce52fe`. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `edd72dff1`, `05d582211`, `1137c9f39`, `9cc5a41aa`, `103c96e62`, `e06c70bc8`, `8a9dd2dc0`, `27c5d4e19`, `65a3fa795`, `25189dea6`, `795af84b4`, `0e3b2c9b6`, `4fc14fb74`.

- **Soundtrack regression coverage**: Verify soundtrack panel sizing and paging through remote keys, and record the Portuguese mockup and native preview proof with device validation limits. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `dd8c54a6a`, `2eb4fecc0`, `4654bdb0f`, `8fb90ad5f`.

- **Regression tracing helpers**: Trace issue regressions from development tools without changing release behavior. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: `234ed9374`.

## Pending (not integrated yet)

- **Glass focus on hero buttons**: Focused Home hero buttons use a glass pill when Glass is enabled. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-heroglassfoco`, `e4d901a1f`.

- **Hero size setting**: Choose how large the Home hero appears, and the reserved layout scales with your choice. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-herotamanho`, `agente/205-porta-perdidos`, `4bada8211`.

- **Trakt device login retry**: Bound login polling by the code expiry and show a retry state instead of looping silently. Platforms: webOS, Android, Samsung .tpk, WASM. Refs: owner request, `agente/205-205-traktlogin`, `71c849ae6`.
