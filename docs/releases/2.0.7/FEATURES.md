# Nuvio Legacy 2.0.7

Everything merged into `integracao/2.0.5` after the `v2.0.6` tag (base `ae9c3c138`), plus the owner requests
that landed in the same window. Last tracking sync: 2026-10-20T02:50:00-03:00. Community server:
**https://discord.gg/9NWr6SHyzJ**

Release engineering is already merged on `agente/205-release207` (`b72189c5d`): version bumped to 2.0.7 in
`deploy/app/appinfo.json` and `tools/tizen-config.xml`, the what's-new card (`src/novidades_cartao.c`) points
at 2.0.7 and `docs/releases/2.0.7/NOTAS.md` holds the GitHub release notes. Before tagging, `repo.json` still
needs the built .ipk (url, sha256, size) and the frost performance line below has to be filled in.

> The in-app card (`src/novidades/207.inc`) is a shorter owner-curated list; this file is the complete release feature source.

---

## Release features (English)

### Highlights

- **Arabic menus and joined captions**: Use the complete Arabic interface with joined right-to-left captions and translated Live TV and scaling controls. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 07deddb4e, ec32d315a, 45016c003, 02da9933e, a427430e1.
- **Choose your upscaling filter**: Select Automatic, SGSR, SGSR sharp, FSR 1, Lanczos-2 or Bilinear on supported GPUs, with live sharpness control for the sharpened filters. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 5fd15e357, d7f22f1a3, 2c5e6afbb, 5869dccb5.

- **Dynamic Stage and Split stay consistent**: Dynamic Home uses Modern Stage and Split geometry while retaining the saved layout identity, background choices and correct fullscreen trailer eligibility. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `1d3728312`, `e9354d12d`, `7d6317655`, merge `dfb22a56e`.

- **Browse files and keep the same release**: Hold OK on a source to browse its complete file pack, choose another copy, continue multipart movies in order, and keep the selected release across episodes with the binge-group preference. Platforms: Samsung .tpk; shared source selection on webOS, Android and WASM. Refs PR #567, `agente/205-pr567port`, merge `d8781e9a8`, `75cf75a13`, `e13dcdb1d`, `66b5094aa`, `e14174c73`, `80c053592`, `366e3df74`, `dd1498e42`, `8a2a29540`, `0cb7d7a47`, `d4c07b768`, `2d13b3b31`, `1e45ce7b3`, `afd16f39b`, `72f9080c6`, `f23d14de9`, `1b081dcac`, `e46711c45`, `64b375d2c`.

- **A real Live TV guide on every platform** (owner request): Live TV and the guide now run on a
  per-source model — add-on, Stalker, Xtream and M3U lists each get their own groups in provider order,
  provider channel numbers, a disk cache and a platform cap, so a huge playlist enters whole groups instead
  of freezing the app. XMLTV grids of any size stream in chunks, keep only the list's channels and a 7-day
  window, and match channels by id, name and alias with fuzzy suggestions and a per-source time offset. The
  guide gains a sources bar with health dots, a group picker per source with a live "N of M channels" count,
  manual guide pick, a last-channel jump, and channel-number entry (digits, 1.5 s or OK). M3U and XMLTV
  URLs can be typed per profile with the TV or phone keyboard, channels play from the stored URL with the
  list's own User-Agent and Referer, and the player OSD EPG uses the same matching cascade. All platforms.
  Refs `f78b5470a`, `e87a44cb2`, `4f8fedfb4`, `c5b52caa7`, `a092fe530`, `695a1bc97`, `00189a790`,
  `bc19c5d32`, `d65bd546a`, `ae9e40109`, merges `ebf14de6b` (`feat/guia-tv`), `ba0639cf0`
  (`agente/205-guiatv`). The guide band opens on DOWN, the guide keeps the video playing behind it, header
  rows were cleaned up and the Options menu got a channel-only preview; late polish on top:
  `a212582f7`, `41b9c25a6`, `07c98b91b`.

- **Variant A Hero layouts**: Wide Hero cards and split Hero rows use the approved artwork and metadata anchors, keep posters visible while scrolling and match the active Dynamic background through transitions. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-heroisA`, `50455c6f9`, `77aa7f216`, `13a5f3bab`, `4ec7e93ee`, `d61e03956`, `124895d8c`, merge `8ff0623e9`; The focused Split row now shows a large logo block instead of a card, with overview text aligned to the last rendered line. Additional refs `6afda5a73`, `f008f67f1`, merge `a228c66f0`. TV validation remains pending.

- **Stage rows show details beside the poster**: Focused Stage posters reveal genres, ratings, synopsis and cast beside the artwork, with an optional spotlight that follows focus. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `a7d5ccb51`, `2dc2e5dc2`, `0749d883e`, `f59c70b50`, `96457c78a`. Stage now includes anchored headlines, timed focus motion and editorial lighting; touch scrolling and viewport bounds remain aligned.

- **Deep links** (#539): `nuvio-legacy://`, `stremio://` and IMDb links open the app straight to a title,
  to search, or already playing, with the newest link kept queued while discovery runs. All platforms.
  Refs #539, `8f38bb943`, `a3eb1e19a`, `7cf145dca`, `f44e64863`, `a4f220cd7`.
- **P2P cache on a USB drive, on every platform** (owner requests, #486 #505): USB caching now shows up and
  works on Android and Samsung .tpk, not just webOS, with the size limit and the reason for each rejection
  spelled out. Android, Samsung .tpk. Refs owner requests, `31a62b273`, `51daddd84`, `b090905dc`, `a633888a3`,
  `ebb5eb8fe`, `9b4f37efb`.
- **External player on Android TV** (#501): pick which installed video app opens the source in
  *Settings › Player* (Internal, External, Ask every time) — plus a separate choice for anime. Subtitle and
  aspect preferences survive. Android. Refs #501, `f954a272c`, `276207fac`, `059f4d4f3`.
- **Automatic TV and Mobile mode in the standard Android APK** (PR #431): the app picks the interface from
  screen size and input, nothing to switch by hand. Android. Refs PR #431, `c305e6f7e`, `8979ef16c`, `875d7f18f`.
- **Subtitle outline reads solid** (owner requests): the subtitle outline is now an opaque 8-direction ring
  instead of a translucent drop shadow, and the chosen opacity applies once instead of compounding at the
  corners. LG webOS, Samsung .tpk. Refs `6a76a46aa`, `d0f6f5986`, `f29e252c2`.
- **Simkl with anime** (owner requests): Simkl's third type (anime) joins the series lists, Continue Watching
  and Plan to Watch, deduplicated. All platforms. Refs `f5dab2f8a`, `d6244a1f1`.
- **Specials (season 0) are shown and playable** (#556, PR #569): anime with a special or out-of-order season
  that Trakt recognises are no longer dropped; the provider's episode numbering survives through the source,
  resume, next episode and Trakt/Simkl/Plex, and the fake per-episode season tab an add-on creates ("The
  Blacklist: Season 20+") is gone. All platforms. Refs #556, PR #569, `8dff0e2a9`, `4a443422b`, `94d16f7de`,
  `288e531ee`, `054ccef24`.
- **Frosted glass is cheaper and looks the same** (owner request): the glass blur now works only on the
  rects the panels read and at most at 960x540, with 3 passes instead of 5 and no stale recapture after a hard
  cut. On the C9 Home at 1080p the blur went from 498,960 to 24,289 pixels per frame and all twelve reference
  shots stay within 4 levels. All platforms. Refs owner request, `4c82273d7`, `93b3d69ec`, `3dc70083c`,
  `013e1341f`, `fe6a04804`, `4dba47b60`.
- **Polished friends row, no "See all"/"+ Add"** (owner request): all six friends row styles now use the
  standard Home focus (no red outline), landscape posters with text in the right place, an initial-letter
  avatar when there is no photo, a fallback poster when there is no art, and the two row buttons are gone.
  All platforms. Refs owner request, `8806e6aff`, `d95bd32e1`, `e27514693`, `f21d993b2`.
- **Mark as watched works** (#548): marking a whole series or one episode sticks after an older whole-series
  job or retry answers later, and confirmed marks survive a restart and a stale Trakt pull. All platforms.
  Refs #548, `66a608ef7`, `9bc31cc9d`, `6db247078`, `8330673a8`, `2d04fbecc`, `b074d9fd5`, `91da5935e`.
- **Player opening screen** (owner request): the logo slides to the centre and the Play pill morphs into a thin
  progress ring below it, over sharp artwork with a dark veil rising to half the screen; "Arte" is the new
  default, with "Arte borrada" and "Desligado" kept. The new **Estilo** setting picks **Centro (A3)** or
  **Canto (B3)**, logo bottom-left with a thin ring under it. All platforms. Refs owner request,
  `718e61abb`, `ace1fd53a`, `922500ef4`, `9c3ecdf85`.
- **Fixed Gray or Colour background in the Dynamic layout** (owner request): the Dynamic ("Apple TV")
  layout background is no longer art-only — Gray and Colour are now options, picked with the accent palette
  picker the settings already used, with a calmer Immersive mode. All platforms.
  Refs owner request, `6da1fc418`, `e91329dd2`, `0cf805980`, `e3fdbe717`.
- **Sidebar frost and Home frost islands** (owner request): the sidebar frosts over screens that carry
  glass, the accent pill becomes tinted glass in glass mode, the frost islands are no longer drawn under the
  Home sidebar glass, and modals only capture the region the panel reads — including the "Saved" modal.
  All platforms. Refs owner request, `90b0f9cd7`, `61764f975`, `fb4af3ee5`, `f087f4b59`, `7bf4caac9`,
  `e3eb56d39`, `37687dce0`, `90c8d380d`, `ab0e9a597`.
- **Sources sheet grows from the clock**: the sources sheet now opens out of the clock island, with the same
  spring and material as the rest of the interface. All platforms. `fbe06f5e7`, `ae39b748a`.
- **See all grid** (PR #568 #563): showcase rows in collections and the search results of each catalog open
  into a grid of every item, without the row limit. All platforms. Refs PR #568 #563, `4baf783fa`,
  `e0ddeca5f`.
- **Catalog outside the quota refreshes right away** (#545 follow-up): a row you just added renews on its own,
  without needing another action. All platforms. `975638744`, `53b050753`, `b03502f84`.
- **Search and episodes work again** (#555): exact-title series like "Bigg Boss" are no longer cut by the
  result cap, and series that come from an add-on with no episodes get their list filled from the catalog.
  All platforms. Refs #555, `2e5b2ffde`, `d479a8463`.
- **Catalogs: ghost rows and vanishing sources** (#545 #551 #552): rows of uninstalled add-ons leave Home, rows
  the add-on does not declare this session no longer take a slot, and sources come back when the add-on probe
  is slow. All platforms. Refs #545 #551 #552 #504, `26b9c635b`, `1c9043ed0`, `455135df2`, `cbcf467f5`, `27988615e`.

### New

- **Movie-source availability alerts**: Get notified when sources become available for a movie. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `00971bdcb`, merge `c77aedbe6`.

- **Browse and filter episode reviews**: View rating distributions, filter comments by score, load more reviews and use a labelled TMDB fallback while moving between episodes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `4407482fa`, `agente/205-eppainel`, merge `ac0bf2ec8`.

- **PGS subtitles on LG webOS**: Watch BD/remux releases with image subtitles through the app overlay when the native player does not expose them. Platforms: webOS. Refs PR #593, @I0-oX, `3fb7c9b3f`, merge `efb9af469`.

- **Prioritize the add-on source order**: Choose “From the addon” to keep eligible sources in provider order across the list, autoplay, next episode and failover. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #618, PR #620, @ghbarker, `e3ff9726d`, merge `ba27808d5`.

- **Saved titles on Home**: Enable the optional Library row below Continue Watching for the current profile on this device. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #507, fcd611bad.
- **Experimental HDR interface**: Opt into Android HDR interface output with verified surface support, HDR10 metadata and SDR fallback when unavailable. Platforms: Android. Refs 855860239, 9f8202c87, 184e6964a, e5fc04b4a, 2e0113eff, ecb3c201a.

- **IPTV connection limits**: Set an account connection policy and serialize Android stream changes to respect limited subscriptions. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `5cf11bf8c`.

- **Episode reviews and redesigned episode cards**: Open episode comments from the context menu and browse Modern episode cards with consistent detail information and dotted focus. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `5bf8bab51`, `10351a387`, `e53b13c06`, `cdde3f208`, `e784e3058`.

- **Darker Dynamic Home and animated top bar**: Choose a darker gray background and see the Dynamic top bar animate with a streaming hint. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `21eb130ab`, `bb583c381`.

- **Episode comments beside the menu**: A long-press episode menu adds episode reviews and rating context; now integrated. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs owner request, `agente/205-epcoment`.

- **Dark gray Dynamic background**: An additional darker background choice preserves existing saved color meanings; now integrated. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs owner request, `agente/205-fundoescuro`.

- **IPTV accounts with limited connections**: Per-account connection limits and serialized channel changes are integrated; equivalent transport handling across platforms remains unverified. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs owner request, `agente/205-conexlim`.

- **Read complete Trakt reviews**: Open comment cards to read full multiline reviews, reveal spoilers, scroll smoothly and move between adjacent reviews. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-cominteiro`, merge `93570ed75`, `1aae8f38f`.
- **Choose sharper interface scaling**: Supported devices offer explicit TV or app scaling, using sharper integer or fractional scaling while preserving video transparency and saved fallback choices. Platforms: webOS, Android, Samsung .tpk, WASM, subject to GPU support. Refs owner request, `agente/205-ui720`, merge `688f08e53`, `691a26008`, `74a4f88cb`, `f0a87650d`, `9ab2b3781`, `f371a2571`, `f228d3816`, `d44f9a283`, plus physical 4K panel detection behind 1080p desktops and App (4K) as the default (`de97e96e0`, `6b59c886d`, `7ed4baa4b`, merges `bbf42d6a0`, `93dac3e87`, `b26f0eff5`).

- **More Stage artwork styles**: Cortina, Aura, Triptych and Monument offer different Stage artwork treatments, and the focused row moves its label below its cards. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `b5f4f9b18`, `2f60a19ff`.

- **Adjust converted Samsung audio timing**: A device-local audio offset from -500 to +500 ms in 20 ms steps lets you align converted audio without shifting video. Platforms: Samsung .tpk. Refs #578, `3c3aa2f2f`, merge `f8176128d`.
- **More readable Apple TV style sidebar**: At interface scales up to 90%, the Apple TV style sidebar grows 20%, and Streaming expands when focus or a mobile swipe passes Settings. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `5bef8c621`, `b328b1749`, merge `8cb639b60`.

- **Latin American Spanish metadata**: Choose Latin American Spanish for title metadata, while regional account languages survive unrelated settings sync. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `37f32a1f0`, `973e303a8`, `0247f87ba`, `bf27b2846`, merge `26472093a`.

- **Choose episode-card ratings**: Select the title-line rating, IMDb, TMDB, both providers or no rating on episode cards. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `a829c328e`, merge `692e650aa`.


- **Poster cards follow genre visibility**: Small and medium Poster cards hide genre and synopsis when the Home hero genre setting is disabled. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-generocard`, `bfa2f9603`, merge `d4314f5ce`.

- **Choose the Stage backdrop**: Modern Home Stage rows can show Black, Current or Artwork only backgrounds, with the choice saved on this device. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-palcolimpo`, merge `0fa8979c6`, `c1e6553b7`.

- **Lighter artwork accents**: Choose Vivid or Light for the device-local “From the art” accent, with readable contrasting text and translations in all 29 languages. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `5897319c5`, `agente/205-artepastel`.

- **Clearer playback help**: Source-opening errors explain whether to check the add-on/debrid service, internet connection or app/TV, while stall and timeout cards direct you to Settings > This TV > Speed test. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `e704faff5`, `ce4e53ba8`.

- **Clearer title pages with adjustable spacing**: Tint-only cast, trailer and rating cards share the page backdrop, while Compact, Normal and Comfortable row spacing keeps ratings, seasons and cast aligned at different text sizes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `b8e94cf5c`, `5a3f41c11`, `83177ade2`, `79eca720f`, `e0c9ba77b`.

- **Optional accent focus plate**: Appearance offers an accent-colour focus plate, including spotlight and profile rows, disabled by default. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `09e9a00ba`, `cc0222525`.
- **Smoother Sources tabs**: Sources drops its panel shadow and slides the selected tab while fading lists, loading messages and errors, respecting reduced animation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `fe6ebfd6d`, `e7a84b249`.

- **Context menu keeps Home visible**: Home stays behind the context menu, and the enriched poster card is configurable. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `d8e4be39c`, `4759f46fd`.

- **Row title styles**: *Settings › Home › Row titles* picks Classic, Island (title in a glass pill with
  the row icon and item count), Editorial (small uppercase kicker above the spaced title) or Accent (a dot
  that grows into a bar on the focused row, with a "See all" tail). All platforms.
  Refs owner request, `331746034`, `4898972d7`.
- **Arabic in Noto Naskh Bold** (community request): the Arabic UI and plain Arabic subtitles now use Noto
  Naskh Bold at every weight instead of synthetic bold, with the baseline aligned to Latin text; Latin, CJK
  and Thai are untouched. All platforms. Refs community request, `458828591`, `0940ae7bf`.
- **Big poster row in three sizes**: the info-on-poster row gains Small, Medium and Large
  (*Settings › Home appearance › Row style*, left/right like Landscape and 4:3) — Small with one synopsis
  line, Large with three and the spare height turned into width. Medium is unchanged.
  All platforms. Refs owner request, `e50a38ef6`, `cbf6fb021`.
- **Smoked glass over the video** (*superseded*): this is no longer the final look — see "Real glass back
  over live video" in Fixes. All platforms.
  Refs owner request, `6f1c61cb8`, `a691e96fb`.
- **Automatic glass quality**: *Settings › Glass quality* (Automatic, Performance, High) — Performance uses
  only the smoked glass, with no capture or blur; Automatic drops to smoked by itself after 4 consecutive
  150-frame windows over the target frame time. All platforms.
  Refs owner request, `30a932909`, `a691e96fb`.
- **Dolby Vision opening in the new style**: the Dolby Vision Cinema screen now uses the same opening as the
  other loading screens, with Style (Centre/Corner), Background (Off/Art/Blurred), the add-on line, the
  detailed card and reduced animations all applying the same way. All platforms.
  Refs owner request, `d89784e82`, `8f788b25b`, `b59987511`, `ef33525a8`, `4542aeffe`, `abb9e49b9`.
- **Frost in the player and its menus**: the buttons, the control bar, the source/subtitle/audio panels,
  the live Statistics panel and the quick menu / parental guide now use the same frosted glass as the rest
  of the interface instead of a flat panel. All platforms (where the video can be read by GL).
  Refs owner requests, `46e0bd5bd`, `3982ef84b`.
- **Translucent top chrome with frost off**: with Glass on and "Vidro fosco" off, the top bar (and the
  avatar/clock island) is see-through tinted glass instead of opaque. LG webOS. `b5007ae85`.
- **Home follows your changes without a restart** (#468, owner request): reordering or disabling rows in
  Settings and adding a new Jellyfin server/add-on refreshes Home on the spot, and a server with many
  libraries stops showing only four catalogs — the view cap went from 6 to 16, boxsets and untyped views
  are accepted. All platforms. Refs #468, `b0e429762`, `c47a40788`, `08c98b017`.
- **One less button in the player** (owner request): the previous-episode button leaves the series action
  bar; next episode and every other button stay, with focus order and spacing intact. All platforms.
  Refs owner request, `4e6b592d8`, `d51d4e46c`.
- **"P2P cache" row on Android**: the USB drive cache setting shows up and works in the standard Android
  APK, as it already did on webOS. Android. `9b4f37efb`.
- **See all grid in collections**: showcase rows open into a full "See all" grid of the collection.
  All platforms. `4baf783fa`.
- **Exact episode identity in scrobbles**: progress sent to Trakt uses the exact episode identity.
  All platforms. Refs PR #560, `ff1f1aa25`.
- **Series identity kept through lookup and playback** (#524, PR #561): a series stays the same series while it
  is looked up and played, instead of turning into a different title, and the requested episode is the one that
  plays — this covers Continue Watching and the resume cards too. All platforms. Refs #524, PR #561,
  `cdf5bdd88`, `2537db6a0`, `86d194462`.
- **Continue Watching up to 40 items** (PR #538): the native cap went from 12 to 40 and future rows are
  published up to it. All platforms. Refs PR #538, `4cbf5ad5c`, `696a3a6ee`, `cee137de7`.
- **Buffer size** (*Settings › Buffer size*): Automatic, 64, 128 or 256 MiB, always clamped to free RAM.
  All platforms. `42b3718cf`, `62421bfdc`, `35a944d9f`.
- **Split hero row style** (*Settings › Home appearance › Row style*): the focused title becomes a large
  landscape panel with art and synopsis. All platforms. `8655956cb`, `669fa7b5c`, `f8815746a`, `76c6e8eea`.
- **Trailer audio**: *Trailer audio* picks Automatic, Original or any language in the list, and prefers the
  dubbed trailer when one exists. All platforms. `b43655122`, `c831a592f`, `c0ea07466`, `31dcf5275`.
- **Captions in the YouTube trailer** (webOS): the embedded trailer follows the subtitle language.
  LG webOS (wasm). `e23f0476d`.
- **Continue Watching up to 40 items**: the row publishes everything that fits instead of stopping at 12.
  All platforms. PR #538, `4cbf5ad5c`, `696a3a6ee`, `cee137de7`.
- **Animated row style picker**: opening and closing the picker animates smoothly, and a stray tap during
  the close no longer changes anything. All platforms. `92656f2ae`, `84239e65a`, `40b7d8e15`, `86c763a2d`.
- **Clock with Up**: from browsing pages, Up reaches the clock and the resume island (PR #547).
  All platforms. `a7d8b1f58`, `960ec872a`.
- **Reorder add-on blocks on mobile** and **save feedback in place**: Settings in the APK's Mobile mode
  explain what only exists on TV and confirm the save. Android. `4bd7a33cc`, `acc47411f`, `29a516106`,
  `7163e053c`, `510007bdd`.
- **More reliable cast and metadata**: add-on portraits stay when TMDB fails, accented and emoji names no
  longer duplicate, and shallow recommendation backdrops are upgraded at High quality. All platforms.
  PR #540 PR #546, `773c576f7`, `a0e225043`, `946ad1819`.
- **Audio track remembered**: the hand-picked track sticks for the whole show, including after closing the
  app (PR #550). All platforms. `0c2a1ba2c`.
- **English friends log**: the friends row reason no longer dirties the translation scan. All platforms.
  `171e49772`.

- **Library and catalog tabs are real glass** (owner request): the tab strips (Saved/Collection/Lists,
  Movies/Series, the title pill and the remaining segmented controls) now use the same frosted glass as the
  rest of the app, with the selected tab keeping its accent fill. All platforms. Refs owner request,
  `21696fd8c`, `796a3441f`, `a096e5eb8`.
- **Settings at 85% by default** (owner request): the interface scale ships at 85%; anyone who never
  picked a size by hand migrates to 85% while an explicit choice — including a historical explicit 100% —
  stays untouched. All platforms. `6e0e0d644`, `be3406a3a`, `b512c4316` (earlier window: `3f8d304b7`,
  `f22761e28`).
- **High-quality artwork is fetched at the size it needs** (owner requests): at High quality, w342 posters and
  w780 backdrops stored by the catalog are fetched at the size the draw needs, and the 480px small backdrop
  is no longer served for cards up to 640px. All platforms. `ff1cd6dcd`, `44bf4884a`.
- **Long poster URL templates** (owner requests): pasting a long poster URL template on the phone page is no
  longer cut at 1024 characters, and the native keyboards accept the same limit. All platforms. `0950a8eb8`,
  `df7c93aaf`.
- **MKV audio track remembered**: on MKV with a converted track, the hand pick keeps sticking for the next
  episode, even with two identical tracks. All platforms. `3486c616b`.

### Fixes

- **Friends artwork recovery**: Seed missing Friends titles through the shared artwork pipeline; now integrated into the release. Platforms: webOS, Android, Samsung .tpk, WASM. Refs a6e233df0; merge 28b2a8579, owner request, `agente/205-amigosarte`.

- **Public episode reviews**: Load public episode ratings and comments without requiring a linked Trakt account and share context-menu material; now integrated into the release. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 29b977c74; merge 4761cc98b, owner request, `agente/205-epreview1019`.

- **Public title comments without account linking**: Read public Trakt title comments without linking an account, with more space below season tabs. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request; 8ba27a3be.

- **Clearer episode context and ratings**: Episode menus restore rating context, align rating sources with title ratings and keep navigation hints on the right; review cards remain safe when pages arrive during drawing. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request; 99c8cb985, 1d526b6a4, 65cf029c1, ecda96227.

- **Experimental HDR menus disabled**: The Android HDR menu experiment stays disabled even when an earlier installation saved it as enabled. Platforms: Android. Refs owner request; c09e7470a.

- **Compact episode cards and clearer ratings**: Standard cards regain separate artwork and text with ratings beside the air date, while Modern cards use the selected glass material. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `19e3cca2f`, `34641ab86`, merge `c895c3a65`.





- **Embedded ASS in large remux files**: Discover embedded ASS subtitles even when large MKV track headers exceed the initial probe, without matching hidden image tracks to the wrong text track. Platforms: webOS. Refs PR #593, @I0-oX, `44d19129e`, `b738d5b14`.

- **Clear posters and reliable scaling on C9**: Artwork requests match output pixels, while corrected shader slots prevent grey output and preserve video transparency across scaling filters. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `366f2d0b4`, `041ae37a1`, merges `952b13d49`, `0e980dd9e`.

- **Complete profile and session cleanup**: The remaining PR #493 branch is now integrated alongside the earlier scoped-session port. Platforms: Android, webOS. Refs PR #493, merge `17c56dbbd`, earlier merge `415544539`.

- **Live TV account limits, EPG search and shared XMLTV matching**: Ported PR #596/#601/#602 account and large-guide improvements with shared XMLTV schedule matching are integrated from `agente/205-livepr2` (`db0cfe7c2`, `6d2fe5a66`, `d88561af4`, `71a96a9b2`, `dd8827fc8`). Platforms: webOS, Android, Samsung .tpk, WASM. Integrated by merge `9538948c2`.

- **HDR playback keeps controls visible**: Android keeps the interface above video when experimental HDR output is enabled. Platforms: Android. Refs b11dc487a.
- **Playback speed keeps audio working**: Audio is decoded to PCM for playback speed changes. Platforms: Android. Refs #591, 5f6436ecd.
- **Trailers follow your audio preference**: Automatic trailer audio uses the playback language preference instead of the interface language. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #564, 22d34f13b.
- **Dynamic Home keeps its sidebar**: The Apple TV sidebar returns with a streaming hint inside the sidebar and reduced-motion support. Platforms: webOS, Android, Samsung .tpk, WASM. Refs b6ddc0fa2.
- **Live TV keeps provider group order**: Guide categories retain their source order when alphabetical sorting is disabled, and older Tizen builds retain IPTV account storage. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 5aa851535, f083e4ba5.
- **Profile changes isolate background work**: Switching profiles resets stale interactions and keeps requests, credentials and provider settings scoped to their original session; logout clears account provider files. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #488, 5f12d3971, b94f2b9af, a05864ce1.
- **Social suggestions survive anonymous startup**: Locally cached Social suggestions load without login while other accounts remain isolated. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 9f1043aa4.
- **Simkl history stays ordered**: Episode history posts remain serialized so fallback scrobbles wait for the history request. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #459, 3fdc8b243.

- **Video remains visible on Tizen 6.5**: The main window stays transparent so native video can appear behind the interface. Platforms: Samsung .tpk. Refs #612, `24876fd76`, merge `41280e41d`.

- **Skip buttons wait until playback is ready**: Intro and credit skip actions stay hidden until video playback is active and remain available across pause or scrubbing until the source changes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-skipcarga`, merge `116f60053`, `5eda5fa1a`, `f518b8b1a`.

- **Samsung live streams ignore premature completion events**: Tizen playback checks native pipeline state before acting on completion callbacks, preventing early stops on redirected or segmented streams. Platforms: Samsung .tpk. Refs owner request, `agente/205-tpklive302`, merge `26240ab80`, `9ac30c587`.

- **Profile picker keeps movie artwork at minimal GPU effects**: Automatic GPU adaptation preserves the selected profile picker movie backdrop even when effects drop to Minimal. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-perfilfundo`, merge `bb446d679`, `f809c7fe7`.

- **Dynamic top bar labels never stack at startup**: Top bar text measurement skips caching widths when the frame raster budget is exhausted, keeping Dynamic menu labels spaced properly. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-menudin`, merge `ec722b7d8`, `2c080fea9`.

- **Live TV filters and recovery remain stable**: Source-scoped visibility, quality choices, category caps and cached inventory stay consistent; Android avoids false stall restarts and cancels recovery when playback resumes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs PR #597; Live TV filter requests, `a3f2bc99e`, `e461614e5`, `de6d0da30`, `9da4c1f3f`, `61647910d`, `6ad003585`, `5386cf994`, `4a009af28`, `f11f3a8af`, `dc67e64b0`, `18d098046`, `30911f682`, `701a6e6eb`, `7e93a0ff3`, `2e8eb26cd`, `1129643aa`, `5dddb0955`, `3202cfd1f`, `46ee70c5d`, `620c1fa3b`, `6fa547b0d`, `ac13f88b5`, `9abea10c0`.

- **Collection layouts fit your screen**: Wall, Cover Flow and Timeline rows respect available space and text scale, while portrait styles and the style picker stay consistent. Platforms: webOS, Android, Samsung .tpk, WASM. Refs collection layout requests, `2082a369c`, `f12d537e9`.

- **Complete provider watchlists**: Provider lists load completely while respecting Trakt retry delays. Platforms: webOS, Android, Samsung .tpk, WASM. Refs provider synchronization, `68866b52b`.

- **Interrupted movies resume automatically**: Playback can resume from its saved position and renew expired add-on URLs with bounded recovery attempts. Platforms: webOS, Android, Samsung .tpk, WASM. Refs merged player recovery, `0bb48c17c`.

- **Modern episode information and focus**: Modern and Refined episode details receive consistent information and minimal focus styling while Standard keeps its existing focus; now integrated. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs owner request, `agente/205-epinfo`.

- **WGT follow-up hardening**: Further browser timeout, subtitle-copy and decoder recovery changes are integrated; device validation remains pending. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs #607, `agente/205-i607`.

- **Catalog rows after PIN entry**: PIN-protected profiles should load catalog rows at startup and after switching; now integrated. Platforms: webOS, Android, Samsung .tpk, WASM; platform validation pending. Refs community report, `agente/205-pinrows`.

- **Recover device settings without undoing newer edits**: Account recovery restores device preferences after logout or reinstall while retaining newer shared settings and provider scope. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-ajustesnuvem`, merge `5db74fe9f`, `5aee9720f`, `d6e4e2401`.
- **Keep unconfirmed Dolby Vision streams on HDR10**: LG playback avoids the Dolby Vision pipeline until probing confirms usable RPU data, while confirmed streams retain Dolby Vision playback. Platforms: webOS. Refs owner request, `agente/205-dvqueda`, merge `a790444f3`, `aae29b157`.

- **Back closes controls before leaving playback**: Back dismisses the visible player layer first, and holding the button cannot also exit playback before release. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner/community request, `agente/205-voltarplayer`, merge `29260acfe`, `bbe5211df`, `ce09230d7`, `a30e48d52`, `c4fb903c5`; physical TV validation remains pending.

- **Collection tabs request the right catalog**: New movies and series folders encode catalog path segments correctly while preserving add-on options and retryable failures; the reported Kaptain instance still needs device logs. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner/community request, `agente/205-colecaonovos`, merge `3390780c7`, `fb0e4b8d9`.

- **Live TV filters follow published channels**: Open filter panels refresh after catalog publication, retain focus and selected sections, and keep late categories and channels reachable through bounded storage and refetching. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-livefiltro`, merges `a1798ad71`, `4a88a523f`, `d768ee117`, `5d8a1a5ab`, `4434611f1`, `e35ee7ac6`, `e28fd824a`, `ebfbee119`.

- **Live streams recover from stalled playlists**: Android revalidates stalled HLS manifests without restarting playback, while bounded buffering and playlist recovery give live sources time to recover. Platforms: Android. Refs PR #597, merge `f22d70933`, `3c8664c62`, `cf1c9ce9a`, `45ef58c6f`, `fec30da82`, `65eb75e81`.

- **Live and custom sources keep useful results**: Live TV bypasses unsuitable source filters and per-add-on overrides let custom catalogs retain the sources they need. Platforms: webOS, Android, Samsung .tpk, WASM. Refs PR #587, merge `8f1328be0`, `957a5040e`.

- **Retry protected Dolby Vision playback**: After an app update or a Settings retry, playback can retry while retaining the two-crash guard and a translated once-per-session explanation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-dvtrava`, merge `4ca4e72ef`, `75798761c`.

- **Hidden sources explain recovery**: Quick filters show how many sources they hide, offer a press-to-clear action and explain empty results with wrapped recovery instructions. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `5754a7f52`, `f2c9975f5`.

- **Library follows your chosen source**: Saved grids rebuild when the effective source changes, remember the choice per profile, preserve membership through server metadata refreshes, and consistently add or remove duplicate titles through Detail and context menus. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-bibfonte`, merge `e5fc8f342`, `8c85a5fd9`, `899530cda`, `de1f5417b`, `e69c40278`, `8bb9b6f72`, `7bd98ab61`, `54b6033d6`. Additional refs `ab0c06ece`, `63cbe08b6`, `1d3ac4e60`, `c1dde48a7`, `4833c78b6`, `f58a5b03d`.

- **Dynamic Stage honors its background choice**: Dynamic Home now respects Black, Current and Artwork Stage backgrounds with Hero enabled or disabled, while retaining Split artwork and Modern geometry. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `450c20891`, merge `71c53b8e1`.

- **Open the full guide with ordinary remote keys**: During Live TV, OK opens the mini guide and a fresh OK on the current channel opens the full guide; held opening-key repeats cannot accidentally trigger it, and the hint uses your chosen language. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `a1860d530`, `617e8c5ec`, `9bf5ba890`, merge `4f244e2de`.

- **Guide logos recover after failures**: Live TV can borrow eligible M3U, XMLTV and channel-variant logos when provider images fail, with bounded retries for recovery. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `65107e6fc`, `b0a172669`, `20f32990d`, merge `caea5e54d`.
- **Custom poster templates accept optional IDs**: IMDb-only cards can use custom poster services whose TMDB identifier is optional while required identifiers retain their fallback. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #566, `6c3b323b2`, merge `25225cf86`.
- **Sports sources keep their provider type**: Sports items try their original item and catalog types before legacy channel fallbacks, sharing the existing add-on timeout budget. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #588, `b3fde1a13`, `a017d44e0`, merge `3356994e8`.
- **Trakt disconnect and retry remain reliable**: Profile disconnects survive delayed credential pushes, Android honors retry delays, and watchlists show newest entries first. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #610, `0ed0312f0`, `8bd79ad4c`, `4a1e17561`, merge `436ea511c`. Paging continues past titles without IMDb IDs, and OAuth retries respect rate limits. Refs #610, `26f0fef6a`, `324d2b92e`. Paging continues past titles without IMDb IDs, and OAuth retries respect rate limits. Refs #610, `26f0fef6a`, `324d2b92e`.
- **Live TV suggests channel sections for you** (owner request): when a Live TV source has no groups or you ask for a fresh grouping, the app asks the Jev worker to propose sections from your channel names, restores them at startup, filters out unusable candidates before counting them against limits, and keeps them bounded even on giant lists. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `cb6d669eb`, `d48f5132a`, `ab64c3059`, `83f2cca2e`, `fcdfb0207`, `ed6d2615b`, `43dd6ded4`, `83a999ff7`, merge `228b62d8f`.
- **Home artwork crossfades between refreshes**: when Home rows refresh under you, replaced artwork now fades in smoothly instead of popping, and a delayed backdrop no longer appears from black on the same title. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `2e67a1b5f`, `30d0763d4`, merge `01ce50d7f`.

- **Episode tiles retain their resting tint**: Episode text keeps its resting background across focus changes, page fades and glass settings. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `8a61d1c1f`, `a95079a43`, `2d26a3a87`, merge `5404c88ac`.

- **Subtitle emphasis survives formatting tags**: SRT subtitles retain bold and italic emphasis while unsupported formatting is removed before display. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #604, `fc94e9c24`, merge `3ce5b5f22`.

- **Samsung trailer volume survives handoff**: Trailer mute and volume choices survive delayed decoder release when playback changes. Platforms: Samsung .tpk. Refs #603, `bd91a888d`, merge `3645de3bd`.

- **Samsung voice search remains available**: Automatic keyboard mode keeps the native microphone visible while respecting an explicit Off preference. Platforms: Samsung .tpk. Refs #585, `df83e19db`, merge `015e20cbf`.

- **Home rows finish scrolling after focus moves**: Interrupted catalogue scrolling settles at the remembered column so rows and artwork prefetch no longer remain stuck. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #579, `4fe226636`, merge `9cd1ea29d`.

- **Library fills the available width**: Library columns and the header badge stay aligned across text sizes, and watched badges stop flickering on LG C9. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `88c2c7c3e`, `ba954c907`, merges `2f35d2af7`, `c2fa09427`.

- **Debrid can resolve hash-only sources**: Hash-only streams marked not web ready remain selectable when a configured debrid account can resolve them. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #505 context, `d8f1d64b1`, merge `00700a9c3`.

- **Source lists stay clean beneath tabs**: Scrolled sources clip at the list boundary without the dark band below the tabs across player and detail entry points. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `32f51412c`, `ad0d2e044`, `b53c1e55f`, merge `522d34bf0`.

- **Title actions keep comfortable spacing**: Focused actions and reminder circles retain their gaps, while the restart pill opens with smoother timing. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `4e27522e9`, `019fa68a8`, `e320c829a`, merge `007a0607f`.

- **Episode artwork stays aligned at every scale**: Episode backdrop strips clip correctly, glass retains its sampled backdrop, and text and veils remain stable on Mali GPUs. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `2e1a440cd`, `2f5f90c31`, `bbe0ee43d`.

- **Episode ratings keep their own sources**: Episode IMDb scores remain independent, late ratings preserve enabled Letterboxd scores, and your preferred rating order survives reloads. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-notasep`, `4391fd482`, `4705f229a`. Additional refs `664115c2d`.
- **Tizen startup retains the accepted glass path**: Startup uses the accepted lazy glass linking path after experimental shader startup changes were reverted. Platforms: Samsung .tpk. Refs owner request, `agente/205-tpk60preta`, merge `6618b7068`, `291fe8e6d`, `b5498bbcb`; device confirmation remains pending.

- **Account cleanup and anonymous Social startup**: Logging out clears account-specific provider files, anonymous sessions load their Social cache, and reopening a profile invalidates stale session state. Platforms: webOS, Android, Samsung .tpk, WASM. Refs PR #493, merge `415544539`, `a5242ee51`.

- **Per-profile add-on switches stay selected**: Add-on toggles survive restart, remain isolated between profiles and retry only unacknowledged changes without delaying priority downloads. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-addonweb`, merge `b83be0bc1`, `bf5d3a917`, `f331b8d4f`, `992e947d0`. Additional refs `f2078cc99`, `132e22a56`, `e2aceb9ee`.

- **Redirect startup stays suppressed**: Redirected startup retains the runtime suppression flag. Platforms: Vidaa (WASM). Refs owner request, `1104d5083`.

- **Stable trailers on low-memory Android boxes**: Card trailers wait for settled geometry and native layout acknowledgement, recover after surface changes, and cap quality at 720p on low-memory devices. Platforms: Android. Refs owner request, `agente/205-trailercard`, `195c34ec1`, `39fedcaad`, `4e60283fc`, `6521bf321`, `8655f6925`, `9b936193b`, merge `2237e56fa`. Additional refs `bd8b06af3`, `1313001ab`. Additional refs `876b85e24`.
- **List settings stay below the header**: List settings no longer overlap the clock and badge at supported UI scales and rendering resolutions. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-ajusteslista`, `7b50de1aa`, merge `840ba5c2e`. Additional refs `ab203ad87`, `79f3437e5`.
- **Guide source removal keeps navigation consistent**: Removing a source resets focus and closes its group selector, preserves disabled-source caches and favorites, and offers a separate confirmed channel-list reset. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-guialimpa`, `5904cfd41`, `6b026330e`, `f599242b4`, `07aa30106`, `30f9f3b35`, merge `b93f0fc71`.
- **Cast details stay with the selected person**: Rapid actor changes keep the latest selection, ambiguous name matches are rejected, metadata refresh respects TMDB settings, and failed lookups allow retry. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #205, `agente/205-elencoappletv`, `8304d5cd5`, `bf75b70ce`, `dea258fee`, `024ba39af`, merge `d64e32043`. Additional refs `1cde26fce`.
- **Samsung startup adapts to limited memory**: The .wgt selects a smaller fixed WASM heap and worker pool on 1 GB TVs, with bounded fallback and a readable terminal error when recovery cannot persist. Platforms: Samsung .wgt/WASM. Refs owner request, `agente/205-wgtmem1g`, `2b62064b7`, `2342be938`, `2f02edd77`, merge `505452eaf`.

- **TorBox cache labels match the source**: Explicit not-cached status takes priority over misleading names or descriptions, while confirmed cached sources remain playable. Platforms: webOS, Android, Samsung .tpk, WASM. Refs community report, `agente/205-torboxlg`, merge `f2d7c9854`, `70f756353`, `c8610436d`, `37b681acf`, `789323ed7`, `1ff87ff04`.
- **Tizen video returns to full frame**: Ending zoom, entering picture-in-picture or opening another title restores video geometry and retries transient AVPlay reset failures. Platforms: Samsung .tpk. Refs owner request, `agente/205-tizenaspecto`, merge `39338ee31`, `d614812a1`, `acac6e9c4`, `c0b853d6b`.
- **Automatic subtitles retain their language**: Subtitle selection preserves add-on languages and copies subtitle entries safely while playback updates the list. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #607, `f117b8cd5`, `7ab455386`, `ab3433f61`, merge `da2f454b7`; loader compatibility also covers Tizen 4/5.
- **Browser requests finish within limits**: Network deadlines and response-size limits prevent stuck or oversized requests, and fatal runtime failures expose diagnostics. Platforms: WASM. Refs #607, `37b3a3819`, `744fc18a8`, `4b01f2454`, `d775c555f`, `6f3c88792`.
- **Selected TorBox files keep working**: Signed links refresh for the selected file, credentials and caches remain scoped to the account, and DTS-only files can open through conversion with MKV seeking preserved. Platforms: Samsung .tpk. Refs PR #567, `20795b880`, `822be941b`, `a37082f61`, `1078df905`, `643ac294f`, `97cb8e8a6`, `0b633743a`, `93ea28a87`, `48512295f`.
- **Appearance rows retain their settings**: The merged art and scene tables remain aligned in the Rows block. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `d232b34fd`.

- **Safer episode source sheets**: Closing playback and discarding episode sources no longer frees the list while the source sheet reads it. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `67b588a0a`; previous-session player logs also help diagnose crashes.

- **Clean Tizen 4 video overlays**: The overlay uses the actual EGL surface dimensions and clears startup frames transparently to prevent uninitialized pixels appearing over video. Platforms: Samsung .tpk (Tizen 4). Refs owner request, `8c45eb257`, merge `b01f47a64`.

- **Stable release-card artwork**: Each 2.0.7 scene retains its own artwork during transitions, and cold cards stay flat until cached frost is available. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `f37fc0931`, `eeb95ee79`, `7bc00e0bf`.

- **Resting Home actions retain frost**: Home hero buttons reuse cached frost with glass enabled or disabled while focused buttons retain the accent fill. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `a341bf956`, merge `1a04b92a3`.

- **Live TV catalogs remain manageable**: Unselected Live TV catalogs remain visible in catalog management without creating unwanted Home rows. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `479ad3d77`.

- **Quick-menu tiles frost with glass off**: focused quick-menu tiles reuse the shared frost material even
  when glass is disabled, keeping the active accent and tile geometry. All platforms. `507727870`, merge
  `31a597e54` (`agente/205-quickmenuvidro`).

- **Panel kicker and selected tab tint restored**: the title and tab row keep their place with measured
  kicker clearance, and selected pills reuse the accent focus plate under its existing glass gate. All
  platforms. `c69b38468`, merge `31b8412f0` (`agente/205-salvostabs`).

- **TV-side settings survive account sync**: a saved TV-only accent theme is no longer overwritten by a
  web theme, existing web preference keys win over stale TV snapshots, the device row quota persists
  atomically across restarts, and portable settings are restored before any upload — including logout
  round-trips, fresh accounts and offline retries. All platforms. `dfcb0420d`, `79a9edcbe`, `948c6db82`,
  merge `9d7db484b` (`agente/205-ajustessync`).

- **Empty add-on cast filled from TMDB**: when an add-on returns no cast, aggregate TMDB credits fill the
  row while preserving person IDs, portraits and limits. All platforms. `a8e7805f4`, merge `bdc5faee1`
  (`agente/205-elencovazio`).

- **Cast pages always open, even right after a search** (owner request): person IDs that the title credits never returned are resolved on demand off the UI thread, cancelled lookups no longer block the cast row, and reordered results keep their order. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `2cf7bfc0c`, `7559ce4ef`, merge `310f3aa3d` (`agente/205-pessoapagina`).

- **Sources sheet explains hidden torrent sources**: when P2P is off or an add-on's summary is truncated, a muted footer in the sources sheet says how many torrent sources were hidden per add-on, and the advice now matches the actual stream requirements. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `35ff5195c`, `95d1b97a4`, `7a0e5073e`, merge `f246d8e91` (`agente/205-torrentdica`).

- **Timeline text stays clear of artwork**: The text column keeps a 24-pixel gap from scaled artwork across form factors, preventing overlap. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-timelinetexto`, `2ac2c50d6`, merge `99c40f3c0`.

- **Stable layouts at every text size (#205)**: Home, title pages, Search and Settings preserve poster sizes, trailer coverage and fixed screen anchors, while upcoming episode descriptions leave room for release dates and status. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #205, `15e810e9f`, `1ae9bf582`, `ccfb08bac`, `9c94e014a`, `6626d68d6`, `f8b4d650a`, `c2954cf49`, `d3fff84c9`, `dc59d2c72`, merge `cb398fc2f`.

- **Smooth immersive background changes**: Immersive backgrounds blend into frosted glass without abrupt threshold jumps. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `a933eb0fe`, merge `ddc51a964`.

- **Artwork survives failed GPU uploads**: Existing artwork stays visible when allocation, upload, mip generation or warmup fails, while bounded retries allow recovery without repeated glass invalidation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `47a43e9e1`, `67dc816cc`, merge `ddc51a964` (`agente/205-frostpisca`).

- **Discord activity keeps public artwork**: Unsupported poster URLs are converted to public Discord artwork URLs so shared activity can display its image. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `115220cef`, `e476d1707`.

- **Player controls stay translucent**: Video controls, post-play cards, error panels, PiP close and hints respect glass opacity even with glass disabled, while other screens retain their normal surfaces. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `aafbe8739`, `d2ebbd9bd`, `5331d4239`, `e272873d1`.
- **Watched action stays on the selected title (#595)**: Switching titles cannot send a queued watched action to the wrong title in Trakt history. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `4c7d236ba`.
- **Reliable Trakt Up Next (#595)**: Confirmed next episodes remain available when metadata is missing or stale. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `c78f36b02`.
- **Profile artwork survives cache pressure**: The profile picker retains artwork between frames and retries loading when a safe cache slot becomes available. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `f5eb11656`.
- **Clear subtitle-sync feedback**: Subtitle sync explains when no embedded subtitle exists for comparison. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `43d758ded`.
- **Intro markers use the current service**: TheIntroDB v4 requests preserve title addressing and provider episode numbering without reusing obsolete negative-cache results. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `66101da36`.
- **Text scaling preserves artwork**: Search, keyboards, episode overlays, collection rows and View all keep artwork dimensions and navigation aligned when text size changes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `8b1f605d0`, `dab8872c2`, `9dbe1a156`, `ef1b092c4`, `ebb0b1365`, `e24e614a2`, `5ff1d7ef3`, `a22f0dac1`, `f04e6de29`, `93fb92118`, `79a2ec3e0`, `1eb28a937`.

- **Cleaner player panel headings**: Subtitle, audio and episode panels start at their headings without a duplicate small title line. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `0a3eacf26`, `6660e72a8`.

- **Friends rows respect screen edges (#205)**: Friends rows clip to the real screen width when text scaling is enabled. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `ae8d8486d`, `1a3e722bd`.

- **Search keyboard spacing and symbols**: Search gains corrected menu spacing, a restyled keyboard and a dedicated ?!# special-character page. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `f1b3b5e67`, `bc76ee8bc`.

- **IMDb score no longer shown as %** (#590): MDBList sometimes sends the IMDb rating on a 0-100 scale
  and the app displayed "80%"/"80.0"; now only the IMDb value is divided by 10 (8.0), leaving Trakt and
  Rotten Tomatoes untouched. Platforms: Samsung .tpk, Android. Refs `03a144294`, `0d998e33a`, `eea9aac9d`.
- **CNCVerse live channels play again**: long add-on ids (cnc:…, 72+ bytes) were dropped or cut, so
  /stream/ went out truncated and returned 0 sources; ids now travel whole via a short stable alias.
  All platforms. Refs `630292d7e`, `47ad499ad`.
- **Smoother Home trailer transitions**: the trailer cutout fades as Frost returns, avoiding an abrupt backdrop change. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `524de66ac`, `240bc60a0`.
- **Cleaner shuffle focus**: the detail-page shuffle button no longer displays an unnecessary focus tooltip. Platforms: webOS, Android, Samsung .tpk, WASM. Ref `c9c2d3f15`.
- **Cast identities stay distinct**: valid cast entries survive metadata filtering and different Unicode names retain separate identities. Platforms: webOS, Android, Samsung .tpk, WASM. Ref `8bd065a36`.

- **Dynamic background no longer flickers**: with frosted glass on in the Dinâmica layout the deferred
  background is resolved before the frost backdrop copy, so Home stops alternating between the right art
  and colour bands on Android. Android. Refs owner request, `5528dc0ad`, `6a64c2e13`.
- **Split hero without a repeated title**: in the split hero the poster list no longer repeats the title
  that is already in focus. All platforms. Refs owner request, `4428dea18`, `f41a4e154`.
- **Frost on everything that looks like glass**: the quick menu and the parental guide panel now go
  through the shared frosted material, and the frost source survives the hero crossfade (baked title art)
  instead of dropping to the opaque fallback. All platforms. `46e0bd5bd`, `3982ef84b`.
- **Black screen on launch** (#558): optional Samsung keyboard and microphone APIs leave the launch path and
  the failure is surfaced instead of hanging. Samsung .tpk. `cc49341e6`, `a1888d007`.
- **A direct URL plays without P2P** (#552 #505): a source with an http(s) URL no longer disappears because
  of the add-on's `notWebReady`, while torrents still ask for the P2P engine (PR #559). All platforms.
  `d25a3e513`, `6d524f947`.
- **Hostnames and P2P** (#552): case-insensitive full-hostname comparison, userinfo and ports skipped,
  numeric IPv4/IPv6 loopback recognised, so local sources stop vanishing. All platforms. `cbcf467f5`.
- **Profile that never loaded the catalog** (#551): a stale profile session is recovered before retrying
  discovery, and a PIN that completes the choice opens the session immediately. LG webOS. `1c9043ed0`, `a2e9daf91`.
- **Explicit choice no longer lost** (#545): a row added by hand or placed in the queue stays eligible while
  others stay out on quota. All platforms. `21d1783b6`, `67985ea5d`.
- **Fullscreen trailer with a failure**: a dubbed YouTube trailer that fails or stalls falls back to
  Apple/IMDb instead of staying black. All platforms. `5f4caef1c`, `1797b5306`, `1c7455e37`, `a6472f80d`.
- **External player no longer steals the subtitle** (#501): while the "Ask" modal is open, a second Tentar
  tap does not start internal playback. Android. `a04f2a820`.
- **Original trailer language**: the language choice is honoured on the detail page and Home, and the right
  dubbed candidate is ranked first. All platforms. `c831a592f`, `c0ea07466`.
- **Frosted glass far cheaper**: one backdrop capture per frame on Android and .tpk too, instead of 4 copies
  and 20 blur passes per animation; the shared capture is dropped when a modal veil intervenes.
  All platforms. `d98d72026`, `0c77460c1`, `5536ee8da`, `f8bc42b11`.
- **USB drive with an unknown filesystem**: P2P fails closed instead of accepting the volume, and the active
  limit is retained. All platforms. `3b025a959`.
- **Search opens the right title**: identity comes from the current selection instead of the last drawn card,
  and Samsung keyboard startup failure is reported in the UI. All platforms. `a2fd7e41d`, `cc49341e6`.
- **Settings explain a disabled P2P**: before USB cache advice, Settings say why P2P is off, and why P2P cache
  and Space limit are greyed out. All platforms. `2e480806c`, `f170d77ba`, `6d537cb92`.
- **Uploaded log keeps its start**: the 16 KB startup header is back in the uploaded log, which is the part
  that diagnoses. All platforms. `84c1a90b6`, `3b12a5d2f`.
- **Real glass back over live video** (owner request): the player and the panels over the picture blur for
  real again — true translucent glass instead of the smoked slab — with a denser tint and a governor that
  trials once instead of dropping the whole app. All platforms.
  Refs owner request, `ce33fe44a`, `6974f7293`, `db42a0a55`, `0fb1a0b1c`, `04375ab4f`.
- **Focused episode text in a padded glass plate** (owner request): on a series page the focused episode's
  title/synopsis plate now spans the card, pads its text and uses the same glass as the other panels
  (dropping to smoked when quality degrades). All platforms.
  Refs owner request, `df06809f2`, `3b39999e5`.
- **C9 screen no longer blinks with frost on** (owner request): hero-promoted artwork was losing the mipmaps
  the rows also draw — every frame rebuilt the chain on the main thread; the mipmaps now stay.
  LG webOS. Refs owner request, `98bc829e1`, `fc6d35d69`.
- **Many Jellyfin catalogs show up** (#468): servers like AIOMetadata expose one view per catalog; the app
  stopped at 6 and only accepted movies/tvshows, so about four catalogs appeared. The cap went to 16 views,
  with boxsets, mixed and untyped views accepted, and the item budget is split across rows so memory does
  not grow. All platforms. `b0e429762`, `c47a40788`.
- **Kitsu special season without breaking the source**: when the Kitsu add-on does not report a season it
  stays absent instead of becoming 0 — the provider's numbering keeps working in the source, on resume and on
  the next episode. All platforms. `4a443422b`.

### Performance

- **Keep Android 4K fallback responsive**: Restore the previous startup surface after the newer 4K fallback path reduced TCL frame rate in local testing. Platforms: Android. Refs owner request, `b592c1618`, reverted by `3ccd70fc3`.

- **Bound background source checks**: Movie availability checks stop oversized add-on responses at 2 MB to limit memory growth during playback. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `37b6f9b68`.

- **Automatic effects follow sustained frame rate**: Automatic GPU effects reduce after sustained slow reports and recover with hysteresis while preserving manual choices. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `d4c3ad79b`.

- **Responsive account synchronization**: Concurrent provider startup requests avoid unnecessary serialization and settings synchronization removes quadratic work. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `0c40a9cde`, `09acb815b`.

- **Readable resolution with lighter effects**: Full, Light and Minimal effect quality can adapt separately from rendering resolution, preserving frost with a cheaper blur and retaining legacy Mali-4xx safeguards. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-gpu1080`, merge `f6d5a0d3f`, `0aaf94305`, `ef6ce1952`.

- **Stable Stage artwork transitions**: Stage textures stay within 1280 pixels while outgoing artwork remains visible until its replacement is ready, with stable row geometry and a clean fade when artwork is missing. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `af885e136`, `ef20235bb`.

- **Responsive cast and source sheets**: Episode navigation stays within its cast budget, invisible ratings skip layout work, and closed source sheets avoid filter scans. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `08d0ff9cb`, `294839b0e`, `aed242219`, `f8c75ff6a`.

- **Glass shader startup stays outside interactions**: Cold shader linking is deferred from interactive rendering, with a flat fallback during startup or failed linking on supported TV builds. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `8466285da`, `35b1edbf6`, `7a3845fdf`, `fc8fb150d`, `bad8e3674`.

- **Recover image workers after decoder failure**: Direct image jobs are reclaimed when their decoder channel fails, allowing subsequent artwork work to proceed. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #607, `93009d304`.
- **Keep Hero and file-browser work bounded**: Hero A stays within its whole-frame baseline budget and idle file-browser rows are reused between updates. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `c3985174d`, `0b633743a`.

- **Lower Hero rendering cost**: Hero artwork uses fused veils and a specialized shader, avoids shading flat backgrounds and draws split captions without extra render passes to reduce Home rendering work. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `4a7a56a1d`, `07ce4a2ff`, `4b2eb36c4`, `8d84be9a1`; target-device performance remains unconfirmed.

- **Glass stays smooth when text caches change**: Retiring unrelated textures no longer rebuilds an unchanged glass backdrop, preserving the existing frost appearance. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `3e8becc0e`, `59efaef75`.

- **Playback buffers adapt to demanding streams**: Android sizes buffers to bitrate within memory limits and can try another available source after sustained throughput problems, with corrected policy handoff and prepared buffers. Platforms: Android. Refs `0d3e01a5f`, `c91106066`.
- **Stronger low-memory relief (#415)**: Playback cache limits follow available memory, release decoded buffers under pressure and gate retries after pipeline errors. Platforms: webOS. Refs `87977f537`, `370769c09`, `bcdd3eb04`, `f77eeeef7`.

- **Smoother title-to-episode transition**: Detail frost no longer re-blurs its backdrop every frame while moving between title and episodes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `fcd6c995b`, `f420d6d0e`.

- **Memory relief on 2 GB webOS TVs (#415)**: Memory pressure flushes subtitle caches, reduces PiP memory floors and caps playback allocations using available memory. Platforms: webOS. Refs `8feec1a91`, `549e2273c`.

- **Frost only on the rects the panels read**: with the Dynamic background deferred, the backdrop copy is
  resolved once per hash change instead of per frame. All platforms. `5528dc0ad`.
- **Much lighter Home and glass on Android (TCL) and .tpk**: one backdrop capture per frame instead of
  4 copies and 20 blur passes (4.00 captures / 20 passes before, 1.15 / 5.75 now). Android, Samsung .tpk.
  `0c77460c1`, `d98d72026`, `5536ee8da`, `d7ec89975`.
- **Continue Watching no longer blocks Home**: catalogs open while the row is built. All platforms.
  PR #538, `cee137de7`, `cfc15aeab`, `8c99638f7`.
- **Buffer clamp accounts for active DTS**: the in-use range reserves its 4 MiB before queue capacity is
  chosen. All platforms. `62421bfdc`.
- **Frosted glass: regional blur and 3 passes instead of 5**: only the rects the panels read are blurred, the
  target stops at 960x540 (480x270 in one pass) and each target is cleared before its pass; a hard cut recaptures
  on the same frame instead of waiting. Animated/focused C9 Home at 1080p: 498,960 → 24,289 blurred pixels per
  frame; at 4K 1,995,840 → 56,621. Extra cost: one full RGB texture saved per layer (5.93 MiB at 1080p).
  All platforms. `93b3d69ec`, `3dc70083c`, `a0f765d7f`, `17717812d`, `013e1341f`, `fe6a04804`, `4ae3e7211`.
- **Glass interior skips the rim math**: most of a big panel used to return after computing the edge normal and
  rim weights it never uses; the frosted path solves density and returns first. Same output. All platforms.
  `4dba47b60`.
- **Frosted glass from pre-blurred art** — *reverted in this window*: the glass blur started reading a half-res
  copy of the backdrop layer alone (1.15 → 0.35 captures per frame), but on the TCL the owner found the previous
  look prettier and lighter and the merge was undone (`f40a93dc3`); the optimization of the current path landed
  instead (`4c82273d7`). All platforms. `e9c809475`, `ab02d4aaa` (the path that left).
- **Home reacts to reordering/enabling rows without reloading everything**: the network refresh that follows
  closing Settings runs once, and server rows over the Home cap are clamped instead of dropped. All platforms.
  `b0e429762`.
- **Detail: fewer full-screen layers**: the trailer card's hole and veil are drawn in one pass, and a
  neighbour card almost off screen no longer counts as full — 4 → 2 layers, fill 1.81 → 0.99 on the harness,
  PNGs byte-identical. All platforms. `5ed6ddfaa`.
- **Changing Home rows copies far less** (owner request): while the hero crossfades, the glass copied the
  whole screen just to blur a few rectangles; it now copies only those rectangles — copy cost per capture
  drops from about 1.0 to 0.13 screens. It does not fix the idle 40 fps or the 4.7x fill spikes on the TCL
  (that needs on-device GPU timing). All platforms. Refs owner request, `e70adbf7a`, `d7522dd17`.
- **Title → season tabs no longer stutters** (owner request): the transition that hit 71.8 ms/frame on the
  TCL with 163 blur rects now grows cheap glass and culling; and the preferred source stops being matched
  every frame (the `[fonte]` log alone wrote 2,347 lines in 2 min) — computed once per list, logged once.
  All platforms. Refs owner request, `df06809f2`, `e5eb23eba`, `3aec9a8c0`, `3b39999e5`.
- **Faster source opening on Android** (@Tolu-Walop-E, PR #572): the end-of-file index is read over the
  parallel connections instead of a serial probe, later requests open straight at the add-on's known
  destination, and a remembered destination that serves another file is no longer trusted — the stream
  starts sooner without a wrong probe. Android. Refs PR #572, `cc61bfdd8`, `809bd7c96`, `8876c13c6`,
  `f6618db6b`, `22f3d22bb`.
- **Trakt comment cards cost zero glass-shader pixels**: up to 8 detail cards of 600x340 each took their own
  frosted read (~0.80M glass-shader px/frame); they now use the flat chip material with a top highlight —
  0 glass-shader px, rim and highlight still visible. All platforms. Refs `bb9f2abc9`, `d4317aaa4`,
  `24047051b`.
- **No full-screen blur when glass is off**: the zero-region fallback copied and blurred the whole screen
  on every backdrop change even with the glass interface disabled; the fallback is now gated on
  `ajustes_vidro()`, and when a frame has no glass the screen is blurred only once. All platforms.
  Refs `033c3fa70`, `0723edb59`, `3f95a9241`.

### Community

- **Remux subtitles from @I0-oX**: PR #593 improves embedded ASS discovery and adds PGS overlay playback on LG TVs. Platforms: webOS. Refs PR #593, `44d19129e`, `3fb7c9b3f`.

- **Add-on order from @ghbarker**: PR #620 adds an explicit source-order preference while preserving the existing priority modes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #618, PR #620, `e3ff9726d`.

- **Source audio and subtitle language labels**: Sources display their declared audio and subtitle languages to help choose the right release. Platforms: webOS, Android, Samsung .tpk, WASM. Refs @ghbarker, PR #487, `e4fb8c085`, `agente/205-pr487v2`, merge `abf3c0fa8`.

- **Live playback and source controls from @ghbarker**: Live sources gain filter overrides and Android HLS stall recovery. Platforms: webOS, Android, Samsung .tpk, WASM; HLS recovery on Android. Refs PR #587, PR #597, `957a5040e`, `3c8664c62`, merges `8f1328be0`, `f22d70933`.

- **Source-file and playback improvements from @Namer03**: Selected PR #567 improvements bring file browsing, release continuity and more resilient playback into the integrated release. Platforms: Samsung .tpk; shared source logic on webOS, Android and WASM. Refs PR #567, `385b44106`, merge `d8781e9a8`.

- **Automatic TV and Mobile mode on Android** — @community, PR #431 (`c305e6f7e`, `875d7f18f`).
- **Series identity during lookup and playback** — PR #561 (`cdf5bdd88`, `2537db6a0`, `86d194462`).
- **Continue Watching without blocking catalogs** — @ghbarker, PR #538 (`cfc15aeab`, `a748ebf32`, `bc13aca67`, merge `19cb1c982`).
- **Keep add-on cast portraits when TMDB fails** — @ghbarker, PR #540 (`a1eca619b`, `8cc5d723d`).
- **Remove uninstalled add-on rows** — @ghbarker, PR #543 (`11e6e8535`, `0d44804f0`).
- **Full recommendation backdrop and artwork quality** — @ghbarker, PR #546 (`946ad1819`, `66299e5f0`).
- **Reach the clock and resume island with Up** — @ghbarker, PR #547 (`a7d8b1f58`, `960ec872a`).
- **Season 0 Specials kept and playable** — @ghbarker, PR #569 (`8dff0e2a9`, `054ccef24`).
- **Remember the hand-picked audio track** — @Tolu-Walop-E, PR #550 (`0c2a1ba2c`, `2cec223fd`).
- **A direct URL plays without P2P** — @mark-major, PR #559 (`d25a3e513`, `06724b3ed`).
- **Faster source opening on Android** — @Tolu-Walop-E, PR #572 (`cc61bfdd8`, `809bd7c96`, `8876c13c6`,
  `f6618db6b`, `22f3d22bb`).

### Internal

- **Container build helpers and GPU checks**: Build and test scripts share container helpers, container inspection reads CLI identifiers correctly, and upscaling selection has regression coverage. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 8d99073c5, 89a018888, 8c7b840d7, 00855beab, e1171bd42, f6831c9eb, ac1ba9725; PR #487.

- **Retained experiment history**: Tracking retains reverted Imersiva layout prototypes and intermediate HDR and episode material work without presenting them as shipped features. Platforms: webOS, Android, Samsung .tpk, WASM. Refs 5ac4a089d, bc101d7be, 772962327, 01d823c57, fd981c59b, d4916b9e7, a62a0aacd.

- **Samsung builds on Apple containers**: Build the ARM32 package through an ARM64 cross SDK while preserving Linux Docker behavior and paths containing spaces. Platforms: Samsung .tpk. Refs `1c21c17ed`, `5c51cdf22`.

- **Source-order regression coverage**: Checks cover settings, playback, preparation and the three-stream AIOStreams example. Platforms: webOS, Android, Samsung .tpk, WASM. Refs PR #620, #618, `ff5c2b699`, `a093a3894`.

- **Remux subtitle regression coverage**: Track-header relayout and subtitle matching checks cover complete codec headers and rejected image-track ordinals. Platforms: webOS, Android, Samsung .tpk, WASM. Refs PR #593, `205c14995`, `82a3feef1`, `6ab0c1132`.

- **Integration checks and session fixtures**: Restore settings and GPU checks, refresh session fixtures and retain the reverted Imersiva experiment as history only. Platforms: webOS, Android, Samsung .tpk, WASM. Refs eae75315f, db2e3a4ca, 1f88b9e4e, 71fbc7453, eef92bc2f, 281e4431e, c6fb8a58c, b24a2cc5d, 1891742bd, 2a5015dce, 72ad3345e.
- **Optional session linkage**: Isolated builds use weak defaults for optional session collaborators and capture probe state under the matching session identity. Platforms: webOS, Android, Samsung .tpk, WASM. Refs a4c9bc039, d50ca08d0.

- **AIOStreams integration research and 2.1 controls plan**: Capability matrix, user journeys and 2.1 control planning document AIOStreams integration and reverted experimental Imersiva home trials. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-aioestudo`, `agente/205-aiohub`, merges `185a01001`, `d649382e3`, `b1c40966f`, `1dcf51b83`, `74cea5346`, revert `72ad3345e`.

- **Regression coverage and release tracking**: Offline regression checks and tracking document playlist recovery, library behavior, episode rendering, PIN profiles and platform investigation. Platforms: webOS, Android, Samsung .tpk, WASM. Refs #607; tracking and test commits, `902a31ce2`, `72795e98a`, `97ce590bd`, `f08d20086`, `bb0d034ac`, `c6865b7c2`, `430ecc402`, `926051fd1`, `f1941a781`, `2c98dd2e8`, `ac92eacec`, `9da0183ae`, `eb4957bb7`, `60788e07b`, `9d2b509fe`, `a5fef4162`, `03b2c9625`, `95b2ed22b`, `a10cdd65f`, `e1faa6e56`, `0887480be`, `2de330bc6`, `0f3553992`, `74da097a2`, `10d1d2961`, `ddda65d97`, `81326cb55`, `57337bed6`, `0c89d10e9`, `eff214cd9`, `af10dd6d5`, `25c73b48e`, `315a61f12`, `4f66a1c1f`, `e7515c158`, `b5812a3d8`, `151b45208`, `ef77ae6b8`, `05426cdee`, `92bf71c7b`, `fe6a8c0bf`, `1e3c93994`, `ef3c51319`, `2c8e6e96a`, `e978236b6`, `5ff69db39`, `f41907092`, `51e1377d5`, `cbd3a2cd9`, `f9944a132`, `e7b0c9631`, `4821d35d3`, `a648c5d4e`, `2068ab77c`, `c38f8dd45`, `90a434bf2`, `64871e2e4`, `3e9f77e5c`, `af7ab651f`, `ad4ec990b`, `de28147ba`, `b207a8a51`, `af6b12394`, `9d89c6f60`, `995376c07`, `45f2397fd`, `8df743dff`, `61205f62e`, `dad0421d3`, `6304d2765`, `02c2ef1d7`, `97adfdd8a`, `7e4b2605b`, `b280cd698`, `1b5c9d39a`, `3118a0494`, `180241442`, `620f041d8`, `9b86a6af8`, `61417ac95`, `82dad7d43`, `ba38888fc`.

- **Samsung startup diagnostics (#612)**: Samsung builds reject incomplete EGL startup state and preserve early diagnostics; the integrated Tizen 6.5 transparency fix still awaits device confirmation. Platforms: Samsung .tpk. Refs #612, `f50314f8b`.

- **webOS Rust MKV build support**: ARM builds enable the offline soft-float Rust core with a C opt-out and repaired subtitle harness dependencies. Platforms: webOS. Refs `agente/205-mkvwebos`, merge `2e38d5397`, `b50df8df6`, `dc39713e9`.
- **Scaling reference checks**: Paired captures identify scene, source, target and filter, reject duplicate images and isolate resolution migration fixtures. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `30ab43b99`, `30319bec0`.

- **Rust MKV core enabled for webOS builds**: ARM builds now use the offline soft-float Rust MKV core by default, with the C implementation retained as an opt-out; playback on a physical TV remains unverified. Platforms: webOS. Refs owner request, `agente/205-mkvwebos`, merge `2e38d5397`, `b50df8df6`, `dc39713e9`.

- **Isolated resolution migration checks**: Migration tests use temporary user data so existing settings cannot affect their results or be deleted. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `30319bec0`.

- **More useful webOS crash diagnostics**: Crash reports retain firmware modules loaded late and include bounded thread diagnostics, helping investigate playback failures; the reported C9 playback crash remains unproven. Platforms: webOS. Refs owner request, `315f4af48`, merge `8626c596c`, `agente/205-c9queda`.

- **Avoid duplicate Samsung package builds**: Samsung package CI runs only for master pushes that change app code, avoiding duplicate integration builds and repeated cancellations. Platforms: Samsung .tpk. Refs `02989fd6c`, merge `4c8c80e9c`.

- **Live TV filter and Stalker recovery hardening**: filtered-empty Live TV sources recover with automatic retries, the admitted channel cache survives reloads, and guide filters stay consistent across categories. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `d497c3673`, `46732dde2`.

- **Release card and Dynamic Stage regression upkeep**: Release copy and card capacity cover all current items, while Dynamic Stage rendering and source-order fixtures verify current integration behavior. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `1694cc299`, `26bf3bf32`, `3c5cc4eb1`, `c3f4d3e1d`, merge `78d299575`.

- **Release card and rendering regression coverage**: Release notes and translated cards were refreshed, and headless Stage fixtures and guide-logo baseline checks were repaired. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `ed9bae8df`, `418cc57ce`, `1744000b3`.

- **UI regression coverage**: Regression checks cover source-header clipping, action spacing, library scaling, regional-language sync and Samsung decoder handoff. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `b53c1e55f`, `ad0d2e044`, `e320c829a`, `bf27b2846`, `bd91a888d`.

- **Missing community regression probes**: The PR #487 port adds audio and subtitle language sanitizer probes, while PR #493 fixtures cover scoped sessions and profile reopening. Platforms: webOS, Android, Samsung .tpk, WASM. Refs @ghbarker, PR #487, PR #493, merge `24f78dbda`, `47ee6b167`, `404a58996`.


- **Experimental Tizen 3 lite shell**: An unsigned opt-in wasm2js experiment restores remote and text-input bridges; recorded smoke evidence still marks shipping as no-go. Platforms: Samsung .wgt/WASM. Refs `agente/205-wgtlite3`, `f475013e2`, `f2f2ff205`, `4de150e96`, merge `4f4f93488`.
- **List rendering regression coverage**: Resolution and header-overlap checks now exercise actual drawable, target and viewport sizes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `ab203ad87`, `79f3437e5`.
- **VIDAA redirect startup guard**: Generated browser glue preserves startup suppression through ST redirects. Platforms: WASM/VIDAA. Refs `1104d5083`.
- **Appearance row table alignment**: Integrated artwork and scene tables match the Rows settings block. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `d232b34fd`.

- **Subtitle loader and browser contracts**: Checks cover Tizen compiler TLS regressions, Asyncify callers, AVPlay cleanup and URL probe boundaries. Platforms: Samsung .tpk, WASM. Refs #607, `ab3433f61`, `d775c555f`, `6f3c88792`.
- **Guide metadata and tracking upkeep**: Catalog genre escape decoding, guide translations and release tracking remain documented. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `6b0f0659c`, `fa5cf5dce`, `d3b41452f`.

- **Playback report checklist**: Required checks ask for comparable playback, source alternatives, the speed-test file-size recommendation and a six-character log code before filing. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `bbd084ada`, `e65ea73cd`, `e5d2d0ad7`.

- **Isolated discovery fixtures**: Server-only stubs preserve profile-provider ownership and restore Continue Watching regression linking. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `0476f8651`.

- **Hero and timeline regression coverage**: Baseline-failure checks, whole-frame GPU comparisons including cold captions and scrolling, scale checks and hidden-window timeline capture support release verification. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `f10ae91d0`, `df65b38b6`, `8d84be9a1`, `2ac2c50d6`.

- **C9 rendering and subtitle timing checks**: Regression checks cover glass-cache budgets and unchanged frost images, plus production readiness, the complete initial 12-second ASS window, and deterministic prefetch timeout checks. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `527c69cf3`, `5f29a8095`, `b12b8e11f`, `9e778e588`, `3934284c5`, `8e8a90415`, `0661e272d`.

- **Broader visual regression coverage**: Sources shadows, profile-cache pressure and deterministic text-scale fixtures cover more UI states. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `45aef45f9`, `915e360a4`, `d6b0c2363`, `cff9c7e1b`, `15903519f`, `0c0c80d58`, `0d5d0d735`, `04ed0d150`, `207e0a283`.
- **Tracking and release documentation**: Release notes, issue tracking and fixed-width caller documentation stay synchronized. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `733bc65e0`, `442021716`, `0eee6cbc3`, `71a57d7b4`, snapshot HEAD `6a54b2213`.

- **Graphics regression coverage**: frame-hash, native trailer and ANGLE GLES2 probes cover Home artwork flicker; measured Frost and menu pixel checks replace misleading fixtures. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `491f8ce0e`, `698fa7417`, `0f1b20514`, `01603fd5b`, `453c7a73c`, `fff6df801`.
- **Integration fixture repairs**: isolated Mach-O linking, discovery retry, sync handshakes and fallback artwork fixtures remain runnable. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `5b37cecd8`, `ac616c0bd`, `da2c69701`, `f87c41ed7`.
- **Release and provider coverage**: the richer translated release card and PenguPlay fixtures document the merged behavior. Platforms: webOS, Android, Samsung .tpk, WASM. Refs `1c1dfa50e`, `a779b3dcd`, `c32a87bbf`, `68cf2c63b`.

- New tests: `tests/dvabertura_shot.sh` (FAIL before, PASS after) for the DV opening; rows with the three
  sizes on one line, the enum appended at the end and help text in both catalog and collection; monotonic
  geometry of the big poster in both layouts; PNGs of the three sizes in `pgtam_shot` (`e50a38ef6`,
  `331746034`).
- New tests for the Dynamic background flicker (`tests/dina_pisca_shot.sh`) and the split hero (last items
  of the row, no wrap) (`5528dc0ad`, `4428dea18`).
- Smoked glass: the test now requires zero captures and zero blur passes and counts the smoked glasses; the
  baked stand-in blur source in the player was dropped (`6f1c61cb8`).
- Tests covering independent Continue Watching, catalog startup, add-on removal, metadata portraits,
  automatic Android mode, player limits, the clock, and registry ownership (`8c99638f7`, `bd6e61119`,
  `a06447082`, `8979ef16c`, `d570cb3c1`, `85562a66b`, `5f0ad732e`, `5ae92423e`, `bbb74484e`).
- Clean translation scan: English log reasons for friends, rows and USB (`171e49772`, `6375a1520`, `35efbf90a`).
- Dolby Vision profile 7 → 8.1 conversion spec (document only, not implemented) and USB drive preparation
  spec (`4907caa50`, `db03a2bc5`).
- Homebrew `repo.json` aligned with 2.0.6 (`c2e50a932`).

- Deep links: parser, bounded queue and regression coverage (`8f38bb943`, `7cf145dca`, `f44e64863`).
- Simkl anime merged into the series lists, deduplicated by imdb (`f5dab2f8a`).
- Glass: single backdrop capture per hash change (`e9c809475`) plus an NV_FPS harness for the detail page
  (`9356bc16c`).
- Collection header mocks moved out of the default suite as `*_shot` (`eb03a56a4`, `1c3043479`), plus
  session/profile fixtures for Continue Watching and catalogs (`8d8f5c92a`, `8c99638f7`, `840dacb67`).
- Test stubs for the master merge: rows (`c76ed4632`), home (`5a926a863`) and sources/languages
  (`33b0d7de0`).
- Triaged open PRs (`72ba10761`) and ported fixes for search, collections, Trakt and player
  (`e0ddeca5f`, `4baf783fa`, `ff1f1aa25`, `61d73a5bf`).
- New `tests/homerefresh.sh`: Home follows row reorder/disable and new sources without a restart; home and
  collection fixtures repaired after a merge (`c47a40788`, `0bfe5ce71`).
- 1080p frost pixel-difference limit pinned in a test: all twelve C9 PNGs against the old path, masking only
  the live clock digits (`4ae3e7211`); typed-title and late-playback identity tests (`2537db6a0`); an assert
  that friends row style 5 has no "+ Add" column (`d95bd32e1`) and that its touch scroll covers the last
  friend's expanded panel (`e27514693`).
- Settings scale migration to a factory default, only for anyone who never chose by hand
  (`3f8d304b7`, `f22761e28`, `6e0e0d644`, `be3406a3a`), and a pixel proof that the Library/catalog tabs
  carry regional frost (`21696fd8c`, `796a3441f`).
- The `tests/homerefresh.sh` fixture repaired again at merge time: duplicate home stubs and missing add-on
  lock stubs (`c47a40788`).
- Glass merge-window regression closed: `tests/vidro_qualidade.sh` failed at `0940ae7bf`
  ("Alta ignores the governor (frost)"); the fix (`0723edb59`, whole-screen blur once when no glass last
  frame, plus the `ajustes_vidro()` gate `033c3fa70`) is merged via `agente/205-vidroqual` (`3f95a9241`).
- New tests for Naskh Bold (`tests/arabe_negrito_padrao.sh`, FAIL without the `text.c` change) and for the
  Home backdrop copy cost (`tests/homelerdo.sh`, 5 checks, FAIL on the baseline) (`458828591`, `e70adbf7a`).
- New Trakt-comment tests (`tests/traktcoment.sh`, series/episode fixtures, a 180-frame per-frame counter and
  the FAIL->PASS for the series regression), the shared wrap cache for that row and the sources-sheet
  generation token / "Play from beginning" (`5195cd14f`, `1165245fc`, `075fec9a4`, `6a6d5b71b`).

### Pending (not integrated yet)

- **Reliable Android 4K selection**: Preserve 4K selection when HDR, startup work or missing GPU timing makes fallback measurements unreliable. Platforms: Android. Refs owner request, `agente/205-recuo4k`.

- **Episode review panel**: Browse clearer rating distributions, filter reviews by score, load further comments and use a labelled TMDB fallback while navigating between episodes. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-eppainel`.

- **Movie availability alerts**: Request a reminder when playable movie sources become available; review blocks integration pending bounded background processing. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-avisofonte`.

- **Episode card materials**: Apply the selected Modern material and restore the requested Standard card presentation; the latest follow-up fix awaits review. Platforms: webOS, Android, Samsung .tpk, WASM. Refs owner request, `agente/205-epcor1019`.








Tracking snapshot: 19 unmerged branches without running jobs; 10 open issues without jobs or prompts. Newer fixes need review on `agente/p2p`. Queued work and 2.1 deferrals are not lost. Android installation succeeded at `cb7ece972`; C9 installation failed and installed TV HEAD is unconfirmed.

Work on open branches with no merge — **not** part of 2.0.7 as it stands:





- **Deferred experiments and owner answers**: Homebrew and Tizenbrew remain deferred until the owner resumes them; glass mockups and profile experiments remain deferred; owner-answer tracking remains on an unmerged branch. Platforms: webOS, Android, Samsung .tpk, WASM; no confirmed release behavior. Refs `agente/205-homebrew`, `agente/205-tizenbrew`, `agente/205-glassmock`, `agente/205-mock21`, `agente/205-perfis21`, `agente/205-respostas`.
- **Custom subtitle font** — the other half of #501, no branch. Android.
- **Manual subtitle sync** — `agente/205-legajuste`, spec only. Deferred to 2.1.
- **Open PRs with no local branch**: #456/#457/#466/#467/#468/#469/#478/#482 (out-of-scope ones stay in the
  map). Triage, not lost.









### Previously pending, now integrated

- **Cheap frost identical to the one approved on the LG C9** — `agente/205-foscovelho` was **merged**
  (`76bc9d0c3`) and then **reverted** (`f40a93dc3`): on the TCL the owner found the previous frosted glass
  (screen capture + blur) prettier and lighter. The pre-blurred art source (`ab02d4aaa`, `d0789c13a`,
  `50a1ac232`, `35f5f4fc3`) is **not** in 2.0.7; its round-4 review was OK (`j-rev-foscovelho-r4`, 22:08),
  already after the merge. The path that shipped — the screen capture, just cheaper — landed as `4c82273d7`
  and is under Highlights/Performance. All platforms.
- **Detail "more" menu on glass** — `agente/205-detalheglass` was **merged** (`cc87bec32`, `e047eecee`, `9ebdc9a3b`, 10:34) after the
  fixes `0b52916d0`, `d04f21ba4`, `531a36f6e`, `937a69475`, `26dbd9a2a`, `3aec9a8c0`, `aef4e3c58`; the
  rev4 BLOQUEIA review (09:20) came **before** the merge. It is under Highlights now; the merged result
  still needs a review. All platforms.
- **Sidebar frost and glass accent pill** — `agente/205-vidromenu` was **merged** (`ab0e9a597`, 08:11,
  conflicts resolved by the merge) and `agente/205-vidroilhas` was **merged** (`f087f4b59`); both are now
  under Highlights. The rev2 review was OK (`j-rev2-vidromenu`, 06:49) **before** the merge, so the merged
  result still needs a review. All platforms.
- **Fixed Gray/Colour background in the Dynamic layout** — `agente/205-dinfundo` was **merged**
  (`e3fdbe717`, 07:49); rev2 review OK (06:48) after the accent-palette fix (`0cf805980`). It is now under
  Highlights.
- **Text and component scale** — `agente/205-textoescala`, 19 commits out (`e7fa6d67c`, `406359d3a`,
  `bab9c56e3`, `86fed77d7`, `4bfafc814`, `342aafc49`, `54242990a` and others): a new "text and component scale" setting (85–130%) that scales
  text, buttons and components while leaving poster/card sizes alone. **Review BLOQUEIA (06:50)**: the
  option never shows up in Settings › Appearance (P1), cards still resize (P2) and Library rows overlap at
  85% (P2). Owner request. All platforms.
- **Trakt comments with glass, frosted context menu and #562/PR #574** landed in this window
  (`335bd2abc`, `a363d821c`, `dc040ef2d`, `a2ea074c7`, `2c7c85860`, all review OK: `rev-traktcoment`, `rev-ctxgrande`, `rev-pr574`) and
  are already in New/Fixes/Performance. Still to measure on a device: the FPS of the 8 glass panels.
- **Live TV guide, quick-menu frost, tab tint, 85% scale, account-settings sync and empty-cast fill**
  landed in this window (`ebf14de6b`, `ba0639cf0`, `31a597e54`, `31b8412f0`, `7507ec45a`, `9d7db484b`,
  `bdc5faee1`, all with OK reviews `j-rev1434-*`) and are already in Highlights/Fixes.
- **Cheaper frost, polished friends row, PR #561 (#524), glass tabs and Settings at 100%** landed in this
  window (`4c82273d7`, `f21d993b2`, `86d194462`, `a096e5eb8`, `f22761e28`) and are already under
  Highlights/New:
- **Vidaa** — `agente/205-vidaa` was **merged** (`d0bf5ba79`, 10-15 22:10); VEREDITO OK `j-revg-vidaa-6`
  after a run of BLOQUEIAS. It ships in 2.0.7; no longer pending.
- **Different collection header** (owner request) — `agente/205-colhdr` was **merged** (`fec2dc0ac`),
  review OK `j-revg-colhdr`; no longer pending.
### Merged in this window (not pending any more)

Landing in 2.0.7, already in the sections above:

- **A3 loading screen** (`ace1fd53a`), **watch tracking / ghost seasons** (`91da5935e`, `c0dc799a3`),
  **#545 quota follow-up** (`b03502f84`), **deep links** (`a4f220cd7`), **external player** (`281b4af37`,
  `161cdebd2`), **PR #569 Specials** (`054ccef24`), **#555 search/episodes** (`d5ae66d9e`, `438423097`),
  **#551** (`e680776ed`), **#552** (`0d7821a01`), **#558** (`a1888d007`), **#483** (`251632be`),
  **#529** (`f8fbd3bd3`), **row styles / See all** (`4baf783fa`, `d2b4e0b12`, `76c6e8eea`),
  **Vidaa** (`d0bf5ba79`), **collection header** (`fec2dc0ac`), **PR #431 TV/Mobile auto** (`875d7f18f`),
  **USB P2P on every platform** (`0fe8a7731`, `2ed922f03`, `84c1a90b6`), **buffer size** (`35a944d9f`),
  **trailer audio + captions** (`b43655122`, `31dcf5275`), **P2P reasons** (`e21441cd2`, `30b57ccf1`),
  **glass/frost work** (`46e0bd5bd`, `b5007ae85`, `e9c809475`, `0c77460c1`, `d98d72026`),
  **release 2.0.7 bump and notes** (`b72189c5d`, `e275de18a`),
  **#468 Home/Jellyfin views** (`08c98b017`), **previous-episode button removed** (`d51d4e46c`), the
  **opening-screen style setting A3/B3** (`922500ef4`, `9c3ecdf85`), the
  **revert of the cached-art frost** (`f40a93dc3`, undoing `76bc9d0c3`), the
  **cheaper screen-capture frost** (`4c82273d7`), the **polished friends row** (`f21d993b2`), **PR #561**
  (`86d194462`), **glass tabs** (`a096e5eb8`), **Settings at 100%** (`f22761e28`), the **Trakt comments row**
  (`335bd2abc`), the **frosted context menu + Poster menu setting** (`dc040ef2d`), **PR #574**
  (`2c7c85860`), the **Dynamic Gray/Colour background** (`e3fdbe717`), the **Home frost islands**
  (`f087f4b59`), the **sidebar frost / glass accent pill** (`ab0e9a597`), **PR #572** (`22f3d22bb`), the
  **player glass / episode plate / hero mipmap** fixes (`04375ab4f`, `3b39999e5`, `fc6d35d69`), the
  **region-only backdrop copy** (`d7522dd17`) and **Arabic Naskh Bold** (`0940ae7bf`) and the **IMDb 0-100 fix** (`eea9aac9d`), the **flat-glass Trakt cards** (`24047051b`), the **governor/glass-off fixes** (`3f95a9241`) and **CNCVerse long ids** (`47ad499ad`), the
  **text/component scale merge** (`9f0588a80`, `302d2bf4f`, `b0c947387`), the **#557 seek warning**
  (`5cf77f08d`, `d034960e8`), the **#581 audio-focus fix** (`5b0d2a35d`, `4a0b3408d`), the
  **#584 720p Auto trailers** (`2bc1ddb55`, `b89ddc139`) and the **#589 slow-motion GIF**
  (`5ab9fc44c`, `b2aa59ada`), the **B2 detail cards and C9 glass cache** (`e0c9ba77b`, `59efaef75`,
  `3cf9a95af`), the **editorial Stage rows** (`8fe7cc0e0`), the **Discord artwork fallback**
  (`e476d1707`), the **MKV ASS 12 s prefetch** (`918dd51c9`), the **frost crossfade/artwork fixes**
  (`ddc51a964`), the **text-scale anchor set** (`cb398fc2f`, `f0143a81c`), the **Hero A performance and
  top-mask set** (`8ff0623e9`, `e4e86c3f2`), the **timeline text gap** (`99c40f3c0`), the
  **cast-person lookups** (`310f3aa3d`), the **hidden-torrent explanation** (`f246d8e91`), the
  **per-source Live TV guide** (`ebf14de6b`, `ba0639cf0`), the **quick-menu frost** (`31a597e54`), the
  **panel kicker / tab tint** (`31b8412f0`), **Settings at 85%** (`7507ec45a`), the **account-settings
  sync** (`9d7db484b`), the **empty-cast TMDB fill** (`bdc5faee1`) and the **720p-leve native surface experiment** (`fce651f6c`, merged `7d551bc3e`, reverted `e52cd6472` — drew in the top-left quarter on TCL).

### Frost performance (to fill before tagging)

Placeholder carried from `j-release207`: the measured numbers belong here once the frost work is measured on a
device. Points to report: one backdrop capture per frame on Android/.tpk (`0c77460c1`, `d98d72026`, `5536ee8da`)
and one capture per backdrop change everywhere (`e9c809475`).

**Measured (`4c82273d7`, `/Volumes/ExternalSSD/tmp/foscoleve3/medidas.md`):** blur pixels per frame on the
animated/focused C9 Home at 1080p went from **498,960 to 24,289** (native/webOS and Android), at 4K from
**1,995,840 to 56,621**; passes per capture 5 → 3 (4 at 4K); Explore 47,520/59,400 → 11,394/14,243; Settings,
detail and search keep **0 captures** when static and unchanged. Captures per frame and the copied screen
fraction stay the same as the old path (0.35). All twelve C9 1080p reference shots pass with a maximum
8-bit channel difference of **4** against the old path (limit 5), masking only live clock digits. The
every-other-frame throttle was dropped: a changed draw hash always refreshes that frame. Extra cost: one full
RGB backdrop texture per layer — **5.93 MiB at 1080p, 23.73 MiB at 4K** — and a region miss may blur the
saved backdrop on that frame. No TV GPU timing yet; these are Mac-side counters.

### Merged in this window (not pending any more)

Landing in 2.0.7, already in the sections above:

- **A3 loading screen** (`ace1fd53a`), **watch tracking / ghost seasons** (`91da5935e`, `c0dc799a3`),
  **#545 quota follow-up** (`b03502f84`), **deep links** (`a4f220cd7`), **external player** (`281b4af37`,
  `161cdebd2`), **PR #569 Specials** (`054ccef24`), **#555 search/episodes** (`d5ae66d9e`, `438423097`),
  **#551** (`e680776ed`), **#552** (`0d7821a01`), **#558** (`a1888d007`), **#483** (`251632be`),
  **#529** (`f8fbd3bd3`), **row styles / See all** (`4baf783fa`, `d2b4e0b12`, `76c6e8eea`),
  **Vidaa** (`d0bf5ba79`), **collection header** (`fec2dc0ac`), **PR #431 TV/Mobile auto** (`875d7f18f`),
  **USB P2P on every platform** (`0fe8a7731`, `2ed922f03`, `84c1a90b6`), **buffer size** (`35a944d9f`),
  **trailer audio + captions** (`b43655122`, `31dcf5275`), **P2P reasons** (`e21441cd2`, `30b57ccf1`),
  **glass/frost work** (`46e0bd5bd`, `b5007ae85`, `e9c809475`, `0c77460c1`, `d98d72026`),
  **release 2.0.7 bump and notes** (`b72189c5d`, `e275de18a`),
  **#468 Home/Jellyfin views** (`08c98b017`), **previous-episode button removed** (`d51d4e46c`), the
  **opening-screen style setting A3/B3** (`922500ef4`, `9c3ecdf85`), the
  **revert of the cached-art frost** (`f40a93dc3`, undoing `76bc9d0c3`), the
  **cheaper screen-capture frost** (`4c82273d7`), the **polished friends row** (`f21d993b2`), **PR #561**
  (`86d194462`), **glass tabs** (`a096e5eb8`), **Settings at 100%** (`f22761e28`), the **Trakt comments row**
  (`335bd2abc`), the **frosted context menu + Poster menu setting** (`dc040ef2d`), **PR #574**
  (`2c7c85860`), the **Dynamic Gray/Colour background** (`e3fdbe717`), the **Home frost islands**
  (`f087f4b59`), the **sidebar frost / glass accent pill** (`ab0e9a597`), **PR #572** (`22f3d22bb`), the
  **player glass / episode plate / hero mipmap** fixes (`04375ab4f`, `3b39999e5`, `fc6d35d69`), the
  **region-only backdrop copy** (`d7522dd17`) and **Arabic Naskh Bold** (`0940ae7bf`) and the **IMDb 0-100 fix** (`eea9aac9d`), the **flat-glass Trakt cards** (`24047051b`), the **governor/glass-off fixes** (`3f95a9241`) and **CNCVerse long ids** (`47ad499ad`), the
  **text/component scale merge** (`9f0588a80`, `302d2bf4f`, `b0c947387`), the **#557 seek warning**
  (`5cf77f08d`, `d034960e8`), the **#581 audio-focus fix** (`5b0d2a35d`, `4a0b3408d`), the
  **#584 720p Auto trailers** (`2bc1ddb55`, `b89ddc139`) and the **#589 slow-motion GIF**
  (`5ab9fc44c`, `b2aa59ada`), the **B2 detail cards and C9 glass cache** (`e0c9ba77b`, `59efaef75`,
  `3cf9a95af`), the **editorial Stage rows** (`8fe7cc0e0`), the **Discord artwork fallback**
  (`e476d1707`), the **MKV ASS 12 s prefetch** (`918dd51c9`), the **frost crossfade/artwork fixes**
  (`ddc51a964`), the **text-scale anchor set** (`cb398fc2f`, `f0143a81c`), the **Hero A performance and
  top-mask set** (`8ff0623e9`, `e4e86c3f2`), the **timeline text gap** (`99c40f3c0`), the
  **cast-person lookups** (`310f3aa3d`), the **hidden-torrent explanation** (`f246d8e91`), the
  **per-source Live TV guide** (`ebf14de6b`, `ba0639cf0`), the **quick-menu frost** (`31a597e54`), the
  **panel kicker / tab tint** (`31b8412f0`), **Settings at 85%** (`7507ec45a`), the **account-settings
  sync** (`9d7db484b`), the **empty-cast TMDB fill** (`bdc5faee1`) and the **720p-leve native surface experiment** (`fce651f6c`, merged `7d551bc3e`, reverted `e52cd6472` — drew in the top-left quarter on TCL).

### Frost performance (to fill before tagging)

Placeholder carried from `j-release207`: the measured numbers belong here once the frost work is measured on a
device. Points to report: one backdrop capture per frame on Android/.tpk (`0c77460c1`, `d98d72026`,
`5536ee8da`) and one capture per backdrop change everywhere (`e9c809475`).

**Measured (`4c82273d7`, `/Volumes/ExternalSSD/tmp/foscoleve3/medidas.md`):** blur pixels per frame on the
animated/focused C9 Home at 1080p went from **498,960 to 24,289** (native/webOS and Android), at 4K from
**1,995,840 to 56,621**; passes per capture 5 → 3 (4 at 4K); Explore 47,520/59,400 → 11,394/14,243; Settings,
detail and search keep **0 captures** when static and unchanged. Captures per frame and the copied screen
fraction stay the same as the old path (0.35). All twelve C9 1080p reference shots pass with a maximum
8-bit channel difference of **4** against the old path (limit 5), masking only live clock digits. The
every-other-frame throttle was dropped: a changed draw hash always refreshes that frame. Extra cost: one full
RGB backdrop texture per layer — **5.93 MiB at 1080p, 23.73 MiB at 4K** — and a region miss may blur the
saved backdrop on that frame. No TV GPU timing yet; these are Mac-side counters.
