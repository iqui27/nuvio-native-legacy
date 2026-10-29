# AIOStreams Integration Study — Target 2.1

**Research baseline:** 2026-10-19, branch agente/205-aioestudo, TV app commit 26240ab80. AIOStreams docs dated 2026-10-19, source pinned `6ca32ed21e3036d87c35c49b24f2cf507d065ed1` (GitHub main). No live instance, account, key or personal config used. **Do NOT ship any part of this document as release notes; details are for roadmap work only.**

## 1. Complete Capability Matrix

Every AIOStreams feature from official docs mapped to TV client consumption, current implementation status (JA TEMOS/PARCIAL/FALTA), effort (S/M/L) and user value (1–5, 1=edge case, 5=core experience).

### Instance, Configuration and Identity

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Install detection (URL, ID, variant) | Identify addon in manifest, extract base/prefix/mode | JA TEMOS | S | 4 | `src/addons.c:326`, `src/addonurl.h` — manifest already downloaded, reuse |
| Configuration read (UUID/password, raw/effective) | Show current settings snapshot before edits | JA TEMOS (via playback) | S | 4 | User API GET with C auth; PLANO-AIOSTREAMS §2 details contracts |
| Configuration write | Apply edits to preset/service/filter/sort/limit/format/catalog | PARCIAL | M | 4 | User API PUT; needs unlock flow, diff tracking, R0/R1 conflict check, undo (slice 2, PLANO) |
| Health check and status | Display version, instance state, service availability | FALTA | M | 2 | GET /api/v1/health (public), GET /api/v1/status (public caps) — show as diagnostic only |
| Variant and parent config | Show selected variant, respect base+inheritance, preserve parent on writes | FALTA | M | 3 | CEL evaluation only on phone; TV shows active variant, base warning (slice 2) |
| Session management | Unlock with password, timeout, optional remember-device | FALTA | M | 3 | Config-session cookie or C unlock; native keyboard + masked input (slice 2) |
| Change password | Rotate credentials, sign out all devices | FALTA | S | 1 | POST /api/v1/user/password; phone-only in MVP; affects linked installs |

### Content Delivery and Streams

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Manifest with catalogs | Browse titles by source, genre, search | JA TEMOS | S | 5 | GET {manifest}/manifest.json; existing Stremio integration |
| Catalog pagination and extras | Search, genre filter, skip/offset | JA TEMOS | S | 4 | GET {manifest}/catalog/{type}/{id}/{extras}; existing adapter |
| Title metadata and posters | Display title, year, synopsis, poster, IMDb link | JA TEMOS | S | 5 | GET {manifest}/meta/{type}/{id}; existing player/detail reuse |
| Stream listing (torrent, debrid, Usenet, P2P) | Playback source selection, deduped/sorted list | JA TEMOS | S | 5 | GET {manifest}/stream/{type}/{videoId}; `src/stream_parse.c:964` reads cached flag |
| Stream formatting and description | Display quality/language/provider/size/speed | JA TEMOS | S | 4 | Formatter applies template; details shown in source list |
| Subtitle support and multiple tracks | Primary/secondary/ASS handling, offset control | PARCIAL | M | 3 | GET {manifest}/subtitles; offset ruler in place; autosync deferred (2.1) |
| Subtitle language display and selection | Filter and sort by language preference | PARCIAL | M | 3 | Languages in filter config; display in stream list and detail |
| Caching status indicator | Show "cached" vs "uncached" debrid badge | JA TEMOS | S | 4 | `src/stream_parse.c` reads isCached; displayed in source badges |
| Stream speed/resolution/group preview | Show latency, DL/UL speed, release group rank | PARCIAL | M | 3 | Formatter can include; diagnostic /api/v1/search available for TV but not integrated |

### Configuration Controls — TV Accessible

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Enable/disable addon | Toggle preset on/off, manage resource usage | FALTA | S | 3 | Per-preset enabled field; toggle in list (slice 3) |
| Addon order | Reorder source priority for dedup/sort ranking | FALTA | S | 3 | Array order in W; reorder mode UI existing in settings (slice 3) |
| Addon timeout, resource type | Bounded integer input for timeout; resource availability | FALTA | M | 2 | Per-preset timeout/resource; stepper/selector (slice 3) |
| Debrid service selection | Enable/disable RD/TorBox/Premiumize, reorder priority | FALTA | M | 4 | Service array order and enabled flag; credential mask/replace (slice 3) |
| Debrid credential input | Add/replace/clear API key for each service | FALTA | M | 3 | Existing debrid masking (src/debrid.c); blank = unchanged, Clear = remove (slice 3) |
| Cache filter (exclude cached/uncached) | Toggle per service/addon, scoped filters | FALTA | S | 3 | Boolean/scoped toggles; affects playback default (slice 4) |
| Resolution/quality filter | Exclude <720p, 1080p, 4K, etc.; per-content-type | FALTA | M | 3 | Multi-select from enum; mode toggle inclusive/exclusive (slice 4) |
| Language/subtitle filter | Prefer/require language, exclude known foreign | FALTA | M | 3 | Per-language toggle + mode; inherited from profile language (slice 4) |
| Size and bitrate limits | Global and per-resolution; independent/conjunctive mode | FALTA | M | 2 | Stepper input; mode selector; reset-to-inherit removes field (slice 4) |
| Sorting criteria and order | Reorder sort key list, asc/desc per criterion | FALTA | S | 3 | Existing sort-list UI pattern; preserve fallback/inheritance (slice 4) |
| Result limits per scope | Cap results by service/addon/quality/etc. | FALTA | S | 2 | Bounded stepper; minimum 1; mode toggle (slice 4) |
| Deduplication strategy | Enable/disable cross-addon dedup | FALTA | S | 2 | Boolean toggle; affects list uniqueness (slice 4) |
| Seeder range and age filter | Only old/fresh releases; movie/series scope | FALTA | M | 2 | Range stepper; type scopes; informational only (slice 4) |
| Formatter selection | Pick built-in, saved or community formatter | FALTA | S | 3 | List picker; synthetic preview shown; custom edit on phone (slice 6) |
| Formatter preview | Show example formatted stream with current settings | FALTA | S | 2 | POST /api/v1/format with sanitized synthetic payload (slice 6) |
| Catalog visibility | Show/hide catalog from home/search/discover | FALTA | M | 2 | Per-catalog flags; affects manifest resources (slice 5) |
| Catalog order | Reorder catalog row position | FALTA | S | 2 | Array order; reorder mode (slice 5) |
| Catalog discovery refresh | Fetch upstream addon catalog list | FALTA | M | 2 | POST /api/v1/catalogs; show busy/success/error (slice 5) |

### Advanced (Phone-first in 2.1, but TV shows summary)

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Stream expression (SEL) entries | Enable/disable existing named filters by remote; show summary | FALTA | S | 2 | Toggle enabled per entry; show count of rules, preserve inverted (slice 7) |
| Stream expression scoring | Adjust score for ranked expressions | FALTA | S | 1 | Stepper only for existing entry; CEL authoring on phone |
| Config expressions (CEL) and variants | Display active variant, show inherited/parent base | FALTA | M | 2 | Read-only summary; variant authoring/testing on phone |
| Regex patterns and ranking | Show installed rules and ranking; no local authoring | FALTA | S | 1 | Count/status only; URL sync on phone (slice 7) |
| Group conditions and addon logic | Display group name, count, or status | FALTA | S | 1 | Informational only; phone-authoring required |
| Custom formatter source | No local editing; preview and selection only | FALTA | S | 1 | SEL/CEL body stays phone-only (slice 6) |
| Sync'd regex/SEL rules | Display source, URL, status, error | FALTA | S | 1 | POST /api/v1/sync/resolve handled by phone; TV shows fetched count (slice 7) |
| Proxy and failover | Display proxy URL/status, failover addon order | FALTA | S | 1 | Informational; credential/URL edit on phone |
| cacheAndPlay, precache, preload | Show mode (off/partial/full); bounded toggle | FALTA | S | 1 | Toggle existing options; complex rules on phone (slice 4) |
| Metadata and poster service | Select service provider | FALTA | S | 1 | Dropdown from status definitions (slice 3) |
| Branding and ID customization | Show selected ID and logo | FALTA | S | 1 | Read-only; rebranding on phone |

### Content Discovery and Search

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Catalog browse (home, genres, lists) | Reuse existing library/catalog UI; show AIO catalogs | JA TEMOS | S | 5 | Manifest-driven; existing Stremio catalog adapter |
| Search within catalog | Type title, filtered results | JA TEMOS | S | 4 | Catalog extras; existing search integration |
| Anime title mapping | Automatic ID mapping across databases (MAL/AniList/etc) | FALTA | S | 2 | GET /api/v1/anime lookup; display type/ID/season; optional diagnostic (slice 7) |
| ID-based stream diagnostics | Search for streams by title ID; show filtered/errors/statistics | FALTA | S | 1 | GET /api/v1/search with C or I auth; optional TV diagnostic (slice 7) |
| Community templates | Browse and apply public templates | FALTA | M | 2 | GET /api/v1/community/templates (public); template picker/QR (slice 6) |

### Playback, Deduplication and Reliability

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Playback through AIO server | Use installed manifest URL for all stream requests | JA TEMOS | S | 5 | Existing Stremio playback; no new player |
| Deduplication across sources | Single sorted list, no duplicates | JA TEMOS | S | 5 | Server-side; TV displays result |
| Source ranking and sort order | Quality → language → speed → provider | JA TEMOS | S | 4 | Server applies config; TV shows ordered list |
| Provider icon/badge in source list | Show source provider (RD, TB, Torrent, etc) | JA TEMOS | S | 3 | Existing source badges; stream_parse.c reads provider |
| Fallback to next source on failure | If first source fails, try next | JA TEMOS | S | 4 | Existing Stremio protocol and player fallback |
| P2P/torrent streaming (optional) | Play uncached torrents via WebTorrent or VLC | JA TEMOS | S | 3 | Existing stream protocol; P2P optional |
| Usenet streaming | Stream from NNTP provider directly | PARCIAL | L | 2 | AIO built-in NNTP engine; Nuvio can reuse if configured |

### Watch State, History and Progress

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Playback state push (watched/unwatched/progress) | Send play events to tracker addon | PARCIAL | M | 4 | `src/watchstate.c` already pushes; MERCADO queue/retry in place |
| Watch history pull | Load watched titles/episodes from tracker | FALTA | M | 3 | Tracker addon pull route; MERCADO pull slice needed (separate) |
| Episode progress tracking | Remember time watched per episode | PARCIAL | M | 4 | Existing player state; watchstate event; MERCADO delivery complete |
| Continue watching / resume playback | Jump to last watched position | JA TEMOS | S | 4 | Player feature; combined with history from tracker |
| Mark series/season/title as watched | Bulk action on collection | FALTA | M | 2 | Manual mark per title/episode + bulk; route through tracker |
| Persona/profile history separation | Different watch history per household member | FALTA | M | 3 | Media-server persona binding, tracker selection (slice 8) |
| Jellyfin integration for history | Use Jellyfin's play tracking as primary source | PARCIAL | M | 3 | `src/jellyfin.c` auth; playstate/history routes available; media-server config (slice 9) |
| Activity and export | View/download history summary | FALTA | S | 1 | AIO extension APIs; phone-first export (slice 9) |

### Media Server / Jellyfin Integration

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Jellyfin login / library access | Sign in to Jellyfin, browse media as library | PARCIAL | M | 3 | `src/jellyfin.c` integration; OAuth flow; existing library ui |
| Jellyfin user/persona selection | Choose which family member's library | FALTA | M | 3 | Jellyfin users; persona binding to AIO (slice 8/9) |
| Jellyfin playback | Play Jellyfin-managed files directly | PARCIAL | M | 3 | Jellyfin integration; media-server feature contract |
| Metadata from Jellyfin | Use Jellyfin metadata (posters, info) as override | FALTA | M | 2 | Addon-protocol metadata; Jellyfin extension (slice 9) |
| Collections in Jellyfin | Show AIO-created collections as Jellyfin folders | FALTA | M | 2 | AIO protocol extension; Jellyfin support (separate) |
| Watched status sync | 2-way sync between Nuvio and Jellyfin | FALTA | M | 3 | Jellyfin playstate route; watch_state push; MERCADO (slice 9) |

### App Variants

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| Smart TV app (LG/Samsung) | Play in TV's native player | JA TEMOS | S | 5 | Existing LG/Samsung apps; AIO integration via manifest URL |
| Desktop/web app | Browse and play in browser | FALTA | S | 1 | AIOStreams web app is separate; Nuvio web is existing |
| Android app | Browse and play on Android TV / phone | FALTA | S | 2 | Official AIOStreams Android; not our port |
| Offline mode | Download titles to watch without connection | FALTA | M | 1 | AIO download feature; not our scope this cycle |
| Watch together | Sync playback with friends | FALTA | L | 1 | AIO watch-together server; not our scope |

### Configuration Transport and Import

| Feature | What TV client uses | TV status | Effort | Value | Notes |
|---------|-------------------|-----------|--------|-------|-------|
| QR install / link instance | Scan QR to configure addon on Stremio | PARCIAL | S | 4 | Existing QR install flow; AIO-specific URL detection (slice 1) |
| Import configuration from URL | Load shared config snapshot | FALTA | M | 2 | GET /api/v1/user with UUID/I; phone-first template apply (slice 2, 6) |
| Export configuration | Save config to file for backup/share | FALTA | S | 1 | Phone export only; configure page feature |
| Sync configuration across devices | All devices use same config automatically | JA TEMOS | S | 4 | Account sync; Stremio native, not AIO-specific |

---

## 2. End-to-End User Journeys on TV

### Journey A: First Setup (No Typing)

1. **User opens Nuvio, discovers AIOStreams tab / menu item** → hint: "Add a super-addon, control everything from here"
2. **QR in phone Settings/AIOStreams** → camera → instant redirect to instance configure page on phone
3. **Phone:** fill template, save configuration → UUID + install URL returned
4. **Back to TV, QR scanner re-appears** → "Configuration saved, tap to install"
5. **TV displays:** AIOStreams configured, version, instance label, "Ready"
6. **First playback:** existing addon now returns AIO catalog → browse, search, play as normal

**Friction:** None if instance already known. Max 3 minutes total (phone-side only).

### Journey B: Daily Use (Sources, Filters, Autoplay)

1. **TV home:** Existing AIOStreams catalog rows (now deduped, filtered, sorted by config)
2. **Browse → pick title → play** → source list shown; cached badges highlighted, sorted by config preference
3. **Source open:** "Downloading"/"Ready" badge; first source plays
4. **During playback:** If buffer stalls, press Back → choose next source, resume
5. **Episode watched:** marker shown next session; click "Mark episode watched" → tracker addon notified
6. **Series continue:** "Resume from S02E05 at 42:00" card shown on home

**No intermediate configuration needed; filters and sorting applied server-side.**

### Journey C: Daily Management (Filters & Sorting, Dedicated Area)

1. **Settings → Integrations → AIOStreams** or **addon menu → Configure locally**
2. **Overview screen:** "Connected to aiostreams-nightly.fortheweak.cloud, config: Tamtaro Complete, version 2.30.4, 147 sources configured"
3. **Addons category:** list of enabled presets (RealDebrid, TorBox, Torrentio, Usenet, etc.) with status/order
   - Focus addon → OK → toggle/remove/move/edit timeout
4. **Services category:** RealDebrid configured ✓, TorBox configured ✓, others available
   - OK → show credential masked as "●●●●●●" + "Replace" or "Remove" button
5. **Filters category:**
   - Cache filter: toggle "Exclude uncached" (default on)
   - Resolution: multi-select 1080p, 2160p (exclude others)
   - Language: prefer English + Portuguese
   - Size: <10GB per resolution
6. **Sorting:** Quality (desc) → Language (preferred) → Speed (desc)
7. **Format:** select "Minimal" formatter from built-in list; preview shown
8. **Catalogs:** see list (Movies, Series, Anime, etc.), toggle visibility, drag to reorder
9. **Exit area:** "Save changes?" → Yes → busy spinner → "Saved ✓, all devices will refresh"

**Total time: 5–15 minutes per customization.**

### Journey D: Tracking Setup (Persona + History)

1. **AIOStreams area → Tracking category**
2. **Show:** Instance status (push: enabled, pull: disabled), configured trackers (none yet)
3. **TV profile selection:** pick "Main account" or "Kids account"
4. **Available tracker addons:** show list of compatible watch_state addons installed in AIO config
5. **Choose tracker:** select one (e.g., Simkl)
6. **Save:** confirmed, now all watches go to Simkl
7. **Check status:** "Last pushed: now ✓" shown on overview

**After this:** every play event auto-tracked; episode marked watched across devices.

### Journey E: Anime Setup (ID Mapping)

1. **Detail screen for "Jujutsu Kaisen"** → AIOStreams sources show (via search)
2. **Behind the scenes:** AIO mapped MyAnimeList ID to TMDB/TVDB automatically
3. **Diagnostic (optional):** AIOStreams area → Diagnostics → search by title → shows mapped IDs
4. **No user action needed** if AIO template includes SeaDex + anime handlers.

---

## 3. Official AIOStreams TV App: Capabilities We Should Match or Beat

**Official AIOStreams apps (Smart TV, Android):**
- Smart TV version (Samsung Tizen): browse catalogs, search, filter by debrid/cache, sort, play streams
- Android: similar + download + watch-together

**What the TV client should match or exceed:**
1. ✓ Catalog browse and search (existing Nuvio catalog UI)
2. ✓ Stream list with badges (quality, language, provider, cached flag) — existing
3. ? Filter by cache status, resolution, language — TV control needed (slices 4)
4. ? Control addon order and enable/disable — TV control needed (slice 3)
5. ? Change formatter (built-in selection) — TV control needed (slice 6)
6. ? Quick service status / credential management — TV control needed (slice 3)
7. ~ Watch together / share playback — complex, defer to 2.2 or beyond
8. ? Offline / download — deferred beyond 2.1
9. ✓ Jellyfin library integration (metadata, playback) — existing if configured
10. ? Anime-specific ID mapping and sourcing — diagnostic only in 2.1

**Nuvio advantage over official:** unified UI (not separate app), profile/family management, Jellyfin/Plex/Emby, existing player features (subs, audio, captions, keyboard), TV accessibility (no typing for config), settings across TV/phone sync.

---

## 4. Prioritized Roadmap for 2.1: Slices with Offline Proof

**Reference:** PLANO-AIOSTREAMS.md §6 defines slice order, effort (S/M/L), and one offline fixture proof per slice. **Each slice includes existing regression tests.**

### Slice 1 (S): Install Detection + Overview + QR Link

**Deliverable:** Identify AIOStreams addon (custom ID, alias, variant support), show status, generate configure QR.

- Detect installed manifest URL: normal + alias + prefix/variant modes
- Extract base, validate AIO schema
- Show overview: instance label, version, config last-fetched, connection state
- Generate QR: `/stremio/configure` URL, no credentials, fit 134 bytes
- Handle multiple installs with user picker

**Offline proof:** URL/variant/alias table fixture; asserts selected base, no token in QR/logs, read-only unknown versions.

**Existing regression:** `bash tests/addonurl.sh` rc 0, secret-not-logged check.

**Owner-visible:** "AIOStreams" menu item appears when addon installed; shows version and connection icon.

---

### Slice 2 (M): Safe Editing Unlock + Snapshot + Undo

**Deliverable:** Raw config snapshot, lossless draft, review/apply/read-back/undo before any config mutation.

- Unlock with raw password: masked native keyboard
- GET raw config R0: preserve all JSON structure, unknown fields, parent config, encryption
- Optional effective/inherited view (no PUT base)
- Draft: edit in memory, no network
- Review diff: show masked before/after, per-category summaries
- Apply: GET R1 again, detect conflicts, send one PUT, verify response
- Read-back: confirm saved values match intent
- Undo: show last verified R0 backup (in-memory, 15-min expiry)

**Offline proof:** HTTP fixture covers unprotected/protected/encrypted-token/conflict/parent-preservation/missing-response-body/timeout/undo-race; asserts zero unintended PUTs, preserved JSON.

**Existing regression:** Unlock flow does not interfere with existing playback (separate auth context).

**Owner-visible:** When opening AIOStreams area, "Unlock to edit" prompt; after unlock, all edits show "Apply" → confirm → busy → "Saved ✓ — Undo available for 15 minutes".

---

### Slice 3 (M): Addon and Service Management

**Deliverable:** Toggle/reorder/configure installed presets; add/replace/clear debrid keys.

- List installed addons (presets): Torrentio, RealDebrid, TorBox, Usenet, SearchAddons, etc.
- Per-addon controls: toggle enabled, move (reorder), remove, edit timeout/resource
- Preserve unknown addons as read-only
- Services list: show enabled + order
- Service detail: show credential masked, Replace/Remove buttons
- Save as config PUT; preserve local Nuvio debrid keys separately

**Offline proof:** Mixed-preset/service fixture; apply reorder/toggle/credential-replace; asserts outgoing JSON matches schema, Nuvio local keys untouched, encrypted fields preserved.

**Existing regression:** Existing `src/debrid.c` masking and local key storage unaffected.

**Owner-visible:** Settings → AIOStreams → Addons; list shows "Torrentio ✓", "RealDebrid (unconfigured)" with status; toggle on/off, move, or edit credential inline.

---

### Slice 4 (M): Filters, Sorting, Limits

**Deliverable:** Cache/attribute/range filters, sort criteria reorder, result limits.

- Cache filter: "Exclude uncached" toggle, scoped to service/addon
- Attribute filters: resolution (multi-select), quality, language, subtitles, tags, audio
- Ranges: seeder/age/size/bitrate
- Sorting: reorder list of criteria (Quality, Language, Speed, Provider), asc/desc
- Result limits: per-scope (global, by service, by addon, etc.), stepper
- All toggles and steppers; long/regex edits on phone

**Offline proof:** Inherited movie/series/anime fixture; applies edits; asserts arrays order preserved, limits ≥1, only selected paths changed, reset-to-inherit removes field.

**Existing regression:** Existing catalog display and source sort remain correct.

**Owner-visible:** Settings → AIOStreams → Filters; rows for Cache, Resolution, Language, etc.; toggle/select/step through values.

---

### Slice 5 (M): Catalog Refresh and Management

**Deliverable:** Discover upstream catalogs, enable/disable, reorder, set Home/Search/Discover flags.

- Refresh action: POST /api/v1/catalogs with current config, show busy/error
- List catalogs: Movies, Series, Anime, Usenet, Collections, etc.
- Per-catalog: toggle visibility, move, shuffle, merge mode (read-only summary)
- Save as config PUT; preserve unknown catalog modifications
- Manifest refresh after save (existing addon path)

**Offline proof:** Catalog fixture with duplicates/genre-required/merged/removed sources; asserts identity by id/type, full-draft preserved, POST request, exactly one manifest invalidation.

**Existing regression:** Existing catalog display and pagination unchanged.

**Owner-visible:** Settings → AIOStreams → Catalogs; "Refresh" button, list of catalogs, toggle visibility/reorder.

---

### Slice 6 (S): Formatter and Template Browser

**Deliverable:** Select built-in or saved formatter, show preview, template browser with phone link.

- Formatter picker: list built-in (Minimal, Detailed, Custom) + saved formatters
- Selected formatter shown as current
- Preview: POST /api/v1/format with sanitized synthetic stream; show formatted name
- Template browser: show title, description, apply/import actions
- Apply template: open phone with QR (configure page + template param)
- Custom template edit: phone-only QR

**Offline proof:** Formatter fixture; choose saved without losing custom/override defs; preview payload sanitized; template URL has no credential; imported template never locally eval'd.

**Existing regression:** Existing formatter selection and preview unchanged.

**Owner-visible:** Settings → AIOStreams → Formatter; picker shows current, preview of "Minimal" vs "Detailed" examples; "Browse templates" opens QR link to phone.

---

### Slice 7 (M): Stream Expressions, Anime Diagnostics

**Deliverable:** Enable/disable existing SEL entries, display summary of regex/CEL/groups; optional anime/search diagnostics.

- SEL list: show installed named expressions (e.g., "Trash Guides", "Release group filter")
- Per-entry: toggle enabled/disabled, show score (read-only), show inverted status
- Summary view: count regex rules, show sync URL status
- Anime diagnostic: search by title → show mapped IDs across databases
- Search diagnostic: look up streams by ID, show filtered results and errors
- Long edits: phone-only QR

**Offline proof:** SEL fixture disables one entry, preserves regex/inverted/overrides; parses partial-source errors; proves diagnostics issue no playback/preload or repeated requests.

**Existing regression:** Existing title search and playback unaffected.

**Owner-visible:** Settings → AIOStreams → Advanced; SEL entries list (toggleable), Diagnostics sub-area with search/anime lookup buttons.

---

### Slice 8 (M): Watch State and Persona Binding

**Deliverable:** Show tracker status, explicit Nuvio profile ↔ AIO persona binding, display direct vs delegated delivery.

- Tracker status: push enabled/disabled, pull enabled/disabled, last action
- Configured trackers: list addon name, status, errors (if any)
- Nuvio profile picker: "Main account" ↔ AIO persona
- Tracker selection: explicit choice of which addon to use (avoid auto-detection)
- Direct vs delegated: explain if both Nuvio watch_state and AIO tracker active → warning
- Save persona binding to config

**Offline proof:** Two-person synthetic tracker fixture; rejects cross-person/double delivery, 401 stays reconnect-needed, never empties history.

**Existing regression:** Existing watch_state push and Jellyfin integration unchanged.

**Owner-visible:** Settings → AIOStreams → Tracking; show "Push: enabled, Pull: not configured"; select profile, choose tracker, confirm with "Save".

---

### Slice 9 (L): History and Activity (Jellyfin)

**Deliverable:** Optional read/export of history using Jellyfin APIs; manual mark-watched action.

- History UI: list recently watched, per-profile
- Activity: show by date
- Clear history: bulk action (phone-confirmed)
- Mark as watched/unwatched: per-title or per-episode
- Jellyfin token handling: negotiate J auth from existing media-server setup
- Hide unadvertised features (read-only checks)

**Offline proof:** Jellyfin feature/token fixture; proves unadvertised history controls hidden, J cannot authorize User PUT, one manual watched change reaches exactly chosen destination.

**Existing regression:** Existing Jellyfin integration and playstate API unchanged.

**Owner-visible:** Settings → AIOStreams → Tracking → History (if enabled); browse watched, clear, mark-as-unwatched per title.

---

### Minimal 2.1 Delivery (Slices 1–6)

Core experience: detection, overview, unlock/edit, addon/service control, filters/sort/limits, catalog refresh, formatter selection, template browser. All category browsing + phone QR fallback for advanced fields. **Value: 90% of TV users' day-to-day needs without touching phone.**

### Extended 2.1 (Slices 1–8)

Add explicit tracker binding, persona separation, watch_state monitoring. **Value: households using Simkl/Trakt with multiple members.**

### Complete 2.1 (Slices 1–9)

Add Jellyfin history/activity. **Value: Jellyfin-first users.**

---

## 5. Current Implementation Status in Nuvio

Grep results across src/ (26240ab80):

| Component | Status | Evidence |
|-----------|--------|----------|
| Addon manifest detection | JA TEMOS | src/addons.c:326, src/addonurl.h |
| Stream parsing (cached flag) | JA TEMOS | src/stream_parse.c:964 reads isCached |
| Debrid masking | JA TEMOS | src/debrid.c, src/debrid.h:74, src/ajustes.c:3968 |
| Watch state push | JA TEMOS | src/watchstate.c queue/retry, src/watchstate_envio.c send |
| Jellyfin integration | PARCIAL | src/jellyfin.c auth, src/rede.h async HTTP |
| AIOStreams-specific config area | FALTA | Not yet started |
| User API unlock/edit/PUT | FALTA | Not yet integrated |
| Catalog refresh and manifest sync | FALTA | Not yet integrated |
| Filter/sort/limit controls | FALTA | Not yet integrated |
| SEL/CEL diagnostics | FALTA | Not yet integrated |
| Anime ID mapping display | FALTA | Not yet integrated |
| Persona/tracker binding | FALTA | Not yet integrated (see MERCADO) |
| History/activity UI | FALTA | Not yet integrated (see MERCADO) |

**Implementation readiness:** Slice 1 (detect/overview) can start immediately; slice 2 (unlock/edit) requires User API integration; slices 3–6 are UI + API + schema work; slices 7–9 depend on MERCADO (watch_state, persona, Jellyfin) work.

---

## 6. Open Questions for Owner

**Merge of 6 from PLANO-AIOSTREAMS.md §7 + this research:**

### Strategic / Approval

1. **TV vs phone authoring scope:** Does 2.1 require every advanced AIOStreams field editable by remote, or is every category reachable with phone-only authoring (long text, regex, CEL, templates, import/merge) acceptable?
   - **Proposed default:** Bounded TV controls (toggle, multi-select, stepper, reorder) + QR for complex text/URL/expressions.

2. **Release completeness:** Is slices 1–6 (MVP: detect, overview, control, filters, catalogs, format) sufficient for 2.1, with 7–9 (expressions, tracker, history) as extended/2.2?
   - **Proposed default:** 1–6 in 2.1; 7–8 if persona/tracker design from MERCADO is ready; 9 (Jellyfin history) in 2.2.

3. **Instance authentication at scale:** Assume self-hosted or public instance; is read-only TV + configure-on-phone QR acceptable for protected/OIDC instances, or must TV support operator/SSO login?
   - **Proposed default:** No TV operator login in 2.1; hand off to phone for protected instances; support raw C credential unlock if user has it.

4. **Shared config by default:** Do TV edits to base configuration affect all AIO clients (default), or should TV changes target a user-created variant?
   - **Proposed default:** Edit base config with explicit "shared across all devices" notice; offer phone-based variant authoring for TV-only mode (not automatic).

5. **Persona and history source:** Which AIO persona belongs to each Nuvio profile? Should AIO-owned history use existing Jellyfin connection (sync via media-server) or direct tracker delivery (separate)?
   - **Proposed default:** Opt-in binding in slice 8; one delivery route per tracker (no dual-send); MERCADO to define persona contract.

6. **Concurrent editing conflict handling:** Accept shared-base-config writes with upstream's CAS limitation (no If-Match), explicit review, and one-session undo?
   - **Proposed default:** Yes on fixture-verified versions; unknown versions read-only. Strict concurrent-write safety requires upstream AIOStreams change (unlikely 2.1).

### Technical / Integration

7. **Encrypted password in manifest URL:** Current docs show I (opaque bearer in manifest) vs C (raw uuid:password for edits). Should TV support both in Basic auth, or stick to C unlock + native keyboard only?
   - **Proposed default:** Unlock with C (user types raw password once); never persist I or derive C from I.

8. **Media-server user switch:** AIO personas for Jellyfin users can be different people (kids, guests). Should TV modal let you switch persona inline, or defer persona/history separation to phone setup?
   - **Proposed default:** Display current persona + show available list; switch persona on demand (simple POST with Jellyfin token); no PIN entry on TV.

9. **Debrid key persistence:** Local Nuvio keys (RD/TB/PM/AD in settings) must stay separate from remote AIO keys. Can we safely store AIO credentials in existing Nuvio debrid structure, or create a separate "AIO services" store?
   - **Proposed default:** Separate store; never mix local and remote keys; existing masking pattern (src/debrid.c) reused but domain-separate.

10. **Fixture and offline proof scope:** Slices require HTTP-mocked fixtures (no live instance). Is offline table-driven proof acceptable (synthetic URLs, responses, config JSON), or do we need simulated Jellyfin/tracker addons?
    - **Proposed default:** Offline fixtures with mocked User API, status, catalogs responses; Jellyfin/tracker simulated as needed for slices 8–9.

11. **Browser/web app:** Should Nuvio web (if it exists or is planned) also get this AIOStreams area, or is it TV/mobile only?
    - **Proposed default:** TV first; defer web to 2.2 if a web app is in scope.

12. **Feature flag or always-on:** Should the AIOStreams control area be gated behind a feature flag or automatically available when addon detected?
    - **Proposed default:** Auto-on if addon manifest ID matches AIO pattern; gracefully hide if not installed.

---

## 7. Known Risks and Mitigation

| Risk | Mitigation |
|------|-----------|
| Full PUT loses user's setup | Lossless raw snapshot, mandatory pre-write backup, narrow leaf edits, R0/R1 conflict check, GET verification. Undo shows 15-min expiry. Source lacks CAS; concurrency real but visible. |
| Concurrent edits from phone + TV | GET R1 before PUT; show conflict warning; require user to refresh and retry. No blind merge or last-writer-wins. |
| Credential exposure (URL, logs, error bodies) | Memory-only raw password, origin/TLS checks, nested URL/secret redaction, generic QR (134 bytes, no token), existing debrid masking reuse. Installed manifest URL already a broad bearer; risk not new. |
| Undo overclaims recovery | One in-memory snapshot, visible 15-min limit, session-scoped. External effects (tracker push, linked-account changes, history) cannot be undone. Durable backup via phone export only. |
| Protected instance auth | C/O/I/J remain distinct. No operator/key bypass. Phone configure QR fallback for protected/OIDC. Native operator login out of scope. |
| API/schema drift | Pinned source fixtures + deployment version/capability check. Unknown config read-only; preserve unsupported fields. Feature disable (403) is access state, not empty config. |
| Save triggers slow upstreams/linked actions | Background jobs, cancellation per-save, bounded request budget, safe upstream errors, shared-client/linked-push notice. No work while drawing. |
| Wrong person / double scrobble | Explicit persona binding, direct-vs-delegated choice, no auto-detect. Queue identity and MERCADO reconciliation. |
| Manifest URL aliasing | Alias redirect validated (same origin, bounded depth), no Authorization forward cross-origin. Relink required for cross-origin alias. |

---

## 8. What Stays Out of 2.1

1. **Instance administration:** No server restart, cache purge, global blocklists, user management, NNTP provider setup.
2. **Account management:** No create config, delete config, password rotation, account linking (phone-only).
3. **Usenet download/NNTP setup:** Leave to phone or advanced settings.
4. **Regex/SEL/CEL authoring:** Display and toggle existing rules; no text editor.
5. **Template merge/complex import:** Phone-only template wizard.
6. **Linked external accounts (Trakt, Simkl, etc.):** Read-only status; push/link/unlink on phone.
7. **Private tracker configuration:** Leave to phone Sonarr/Radarr setup.
8. **Offline download / Watch Together:** Defer to 2.2.
9. **RTL (Arabic) interface:** Defer to 2.2.
10. **Concurrent edit conflict resolution:** Show warning, no auto-merge.

---

## 9. Validation Against Existing Plans

**PLANO-MERCADO.md** (watch_state, metadata, collections, subtitles, Jellyfin):
- §4–§11, §14 cover watch-state delivery/pull, metadata, collections, streams, subtitles, Jellyfin integration.
- This study assumes those implementations are complete; slices 8–9 depend on them.
- No duplication: this study adds the TV management surface, not the tracking/delivery itself.

**PLANO-ECOSSISTEMA.md** (account sync, device auth):
- §7 prohibits TV config writes; DECISOES.md overrides this for 2.1, explicitly approving dedicated TV area.
- Account sync remains phone-first; TV linking is lookup + QR.
- This study respects credential separation and no cloud expansion.

**DESIGN.md** (Glass UI, approved mockups):
- TV AIOStreams area uses existing Settings/Integrations patterns (glass+solid, category rows, reuse list/modal/stepper UI).
- No new visual system or mockup needed for research; design iteration is separate.

---

## 10. Example Fixture: Offline Proof for Slice 1

```json
{
  "test": "detect-aiostreams-urls",
  "cases": [
    {
      "url": "https://aiostreams.example.com/stremio/com.aiostreams.viren070/manifest.json",
      "expected": {
        "base": "https://aiostreams.example.com",
        "id": "com.aiostreams.viren070",
        "isAIO": true,
        "variant": null,
        "hasToken": false
      }
    },
    {
      "url": "https://aiostreams.example.com/stremio/u/myalias/v/default/manifest.json?auth=token",
      "expected": {
        "base": "https://aiostreams.example.com",
        "id": null,
        "isAIO": true,
        "variant": "default",
        "hasToken": true,
        "warning": "Token present; remove before generating QR"
      }
    },
    {
      "url": "https://torrentio.strem.fun/manifest.json",
      "expected": {
        "isAIO": false,
        "reason": "Not an AIOStreams manifest"
      }
    }
  ]
}
```

---

## 11. References and Pinned Sources

**Official docs (2026-10-19):**
- Complete docs: https://docs.aiostreams.viren070.me/llms-full.txt
- Setup: https://docs.aiostreams.viren070.me/configuration/setup/
- Configuration: https://docs.aiostreams.viren070.me/configuration/options/
- APIs: https://docs.aiostreams.viren070.me/apis/user, /search, /anime
- Media server: https://docs.aiostreams.viren070.me/apps/media-server/
- Smart TV: https://docs.aiostreams.viren070.me/apps/smart-tv/
- Jellyfin: https://docs.aiostreams.viren070.me/apps/jellyfin/
- Watch state: https://docs.aiostreams.viren070.me/reference/addon-protocol/watch-state/

**Pinned source code (commit 6ca32ed21e3036d87c35c49b24f2cf507d065ed1):**
- User API: https://github.com/Viren070/AIOStreams/blob/6ca32ed21e3036d87c35c49b24f2cf507d065ed1/packages/server/src/routes/api/user.ts
- Schema: https://github.com/Viren070/AIOStreams/blob/6ca32ed21e3036d87c35c49b24f2cf507d065ed1/packages/core/src/db/schemas.ts
- Status/capabilities: https://github.com/Viren070/AIOStreams/blob/6ca32ed21e3036d87c35c49b24f2cf507d065ed1/packages/server/src/routes/api/status.ts

**Related Nuvio plans:**
- PLANO-AIOSTREAMS.md — §2 capability inventory, §3 dedicated area design, §4 safe writes, §6 slices
- PLANO-MERCADO.md — §4–14 watch_state, metadata, collections, Jellyfin
- PLANO-ECOSSISTEMA.md — account sync, device auth, ecosystem entry
- DECISOES.md — owner decision to approve 2.1 AIOStreams area (overrides ecosystem prohibition)

---

**Document status:** Research baseline complete. Implementation, UI mockup, fixture authoring, and TV acceptance remain pending. No code, secrets, or account data included. Ready for owner review and implementation planning.
