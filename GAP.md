# Title detail 2.1 round 2 — initial gap inventory

Baseline: `a96875846`, branch `agente/21-detalhe21-resto`.
Contract: `/Volumes/ExternalSSD/tmp/detalhe21/v2/README.md`, round-2 PNGs and `src/pagina.html`.
Statuses describe this baseline, not visual or TV acceptance. The owner's round-2 request supersedes older section order and empty-quote behavior. Existing approved section geometry is retained.

## 1. Settings

| README requirement | Status | Evidence / remaining work |
|---|---|---|
| Title page > What appears > Top of page, first row | Missing | Existing group: `src/ajustes_ux_tela.inc:138`; no top selector. |
| Ready version (default), Personal, Both; profile persistence | Missing | Detail options: `src/ajustes.c:212`; add through existing settings storage. |
| Three previews; current-value description, profile/default/history help | Missing | Existing native preview dispatch: `src/ajustes_ux_cenas_titulo.inc:812`. |
| No spoilers (extends blur) | Partial | Blur getter: `src/ajustes.c:2630`; title/synopsis reveal missing. |
| Where you saw the cast | Missing | Cast section exists; no history setting. |
| Top mode changes only hero; rating remains available below | Partial | Shared body: `src/detail.c:7460`; rating editor belongs to parallel rating branch. |

## 2. Sections, in README row order

| Section / row | Status | Evidence / remaining work |
|---|---|---|
| Movie/series section order | Partial | Tables `src/detail.c:628`, `src/detail.c:637`; trailers/collection and footer differ. |
| Ready hero: play/save/watched/more | Already in code | Action dispatch `src/detail.c:2703`; hero `src/detail.c:7439`. |
| Ready hero: real format chip, loading ring/dot, ready versions island | Missing | Existing source preparation is outside Detail; reuse rather than invent source availability. |
| Ends-at time | Partial | Existing `TituloTempo` integration `src/detail.c:36`; personal combined line missing. |
| Personal hero: never watched/resume/remaining time | Partial | Resume selection `src/detail.c:2196`; dedicated history line missing. |
| Personal/Both: star and saved personal rating | Parallel | `agente/205-205-avaliar21`, log `892f61354`, `ecc7119c8`; do not reimplement editor/bar. |
| Episodes: tabs/cards/rating/unwatched blur | Already in code | Counts `src/detail.c:2050`; episode geometry `src/detail.c:1944`. |
| Episodes: season progress ring and summary | Partial | Season graph `src/detail.c:2182`; tab ring/summary missing. |
| Episodes: future-season reminder tab | Partial | Real future-date guard `src/detail.c:2242`; currently hero reminder. Never fabricate a season/date. |
| Episodes: no-spoiler titles/synopses, hold reveal | Missing | Existing hold dispatch `src/detail.c:2761`; blur alone insufficient. |
| Episodes: per-episode version chip | Missing | Must use episode identity; no fabricated ready state. |
| Your progress: T×E rating grid, watched marks | Partial | Existing season graph `src/detail.c:2182`; grid missing. |
| Your progress: best season/unwatched best episode/pace | Missing | Requires actual episode ratings and history; never invent pace. |
| Ratings: provider blocks | Already in code | `src/detail.c:2178`, `src/extras.h:36`; absent ratings hidden. |
| Ratings: Your rating editor | Parallel | Rating branch `3f70510ea`, `892f61354`. |
| Ratings: friends/recommend action | Partial | Friends getter `src/detail.c:293`, recommend availability `src/detail.c:2228`; existing action reused. |
| Ratings: age classification | Partial | Existing facts `src/detail.c:1915`; no invented content descriptors. |
| Trailers/extras: native playback, watched marker, distinct clips | Partial | Single native playable card deliberately guarded `src/detail.c:2074`; source limitations must stay truthful. |
| Cast: portrait/character, filmography | Already in code / parallel | `src/detail.c:2780`; filmography branch `ee43f932a`; do not port branch work. |
| Cast: monogram fallback/history/header creator | Partial | Cast renderer exists; fallback/history need inspection. Filmography implementation excluded. |
| Same author/collection: release timeline/current/watched/announced | Partial | Real collection data `src/extras.h:124`; existing collection card `src/detail.c:2093`. No guessed author relationship. |
| Universe: release/chronological order | Missing | No curated chronology API. Hide chronology and absent universe; no empty switch. |
| More like this: posters/open title stack | Already in code | `src/detail.c:2742`, `src/detail.c:7495`. |
| More like this: honest reason/header and watched mark | Missing | Related API has title/year/poster, no personalized reason; derive only facts actually known. |
| Comments: likes order | Already in code | `src/extras.c:721`. |
| Comments: three cards, spoiler bars, first OK reveal/second full text | Partial | Cards `src/detail.c:6532`; spoiler flag/reveal missing. |
| Quotes: source-backed only, hide absent block | Partial | Real Wikiquote source exists, but `src/detail.c:2173` reserves empty focus. Must change. |
| Facts: original/local title, release, duration, language | Partial | `src/detail.c:1892`; title/language rows and series footer missing. |
| Production | Already in code | Real company/network logos `src/detail.c:2116`. |
| Full-resolution art / synopsis 24–26px / Up never opens top menu | Partial | Hero/art path exists; row/back navigation needs targeted checks. |

## 3. Focus table

| README row | Status | Evidence / remaining work |
|---|---|---|
| Hero horizontal actions / play / Back to origin | Already in code | `src/detail.c:2703`; top mode chip/star additions missing. |
| Version chip / island | Missing | No chip focus layer. |
| Rating bar 1–10, save/cancel | Parallel | Rating branch owns editor. |
| Season tabs / no-spoiler switch / reminder | Partial | `src/detail.c:2761`; switch/future tab missing. |
| Episode cards / current episode / hold reveal | Partial | `src/detail.c:880`; spoiler reveal missing. |
| Progress grid vertical/horizontal episode navigation | Missing | Existing season graph uses one column per season `src/detail.c:2182`. |
| Ratings providers/personal/friends | Partial / parallel | Current one-stop focus `src/detail.c:2178`; editor excluded. |
| Trailers / return to same card | Already in code | Shared native trailer route `src/detail.c:2359`. |
| Cast / filmography | Already in code / parallel | `src/detail.c:2780`. |
| Collection/universe | Partial | Collection list works; universe absent. |
| Related title stack return | Partial | Index stack `src/detail.c:7495`; row/column restoration needs verification. |
| Comments reveal/full text | Missing | Existing cards focus but no spoiler state. |
| Footer blocks / no dead focus | Partial | Facts one stop, production clickable; quotes empty stop must disappear. |
| Back below hero returns to play; next Back exits | Partial | Existing navigation handler; verify overlays and row memory. |
| Remember columns / skip empty rows | Already in code | `src/detail.c:880`, `src/detail.c:902`. |
| 0.42 inactive alpha / 700ms expo scroll / header at 140 | Partial | Existing spring scroll `src/detail.c:3787`; keep approved geometry. |

## 4. Lazy-load rows

| README row | Status | Evidence / remaining work |
|---|---|---|
| Art/logo/meta first; active-title-only source debounce | Partial | Detail setup `src/detail.c:1466`; reuse source preparation service. |
| First below: active season stills / movie ratings | Already in code | Existing texture demand and section culling; ratings from extras. |
| Load within 1080+400; fade 350ms/cascade 70ms | Partial | Existing reveal arrays `src/detail.c:330`; exact timing differs. |
| Network order ratings/episodes/cast/trailers/collection/related/comments/facts | Partial | `extras_pedir` currently bundles requests `src/detail.c:1466`; avoid duplicating workers. |
| Evict beyond two screens; about 30 live textures | Partial | Shared texture cache included `src/detail.c:57`; no Detail-specific budget. |

## 5. GLES2 rows

| README row | Status | Evidence / remaining work |
|---|---|---|
| Text/quads/arcs; glass only floating controls | Partial | Existing body cards retain historical glass paths; no new body frost. |
| Cached unwatched blur | Already in code | Existing episode shader path. |
| Native trailer, no second decoder | Already in code | `src/detail.c:2359`; preserve platform source guard. |
| Art parallax 0.32 translate | Partial | Existing hero translation; exact factor not established. |
| Progress grid uses no textures | Missing | Implement with existing primitive/text APIs. |
| Scroll translate/alpha without per-frame relayout | Partial | `src/detail.c:952` recalculates layout; existing behavior retained unless necessary. |

## 6. Implementation order / exclusions

Follow README section order, one tested commit per changed section. Baseline inventory is committed before implementation. Filmography (`agente/21-filmo-impl`), glance (`agente/21-glance-impl`), rewatch (`agente/21-rewatch-impl`) and personal-rating editor (`agente/205-205-avaliar21`) are excluded; only their git logs were read. Missing upstream quote/chronology data must hide blocks, not create empty focus. No push, deploy or stash.

## Delivery status after implementation

Implemented and committed:

- Profile-local Ready / Personal / Both selector, native preview, default Ready, original profile migration guard; same lower document in all modes.
- Source format chip describes the actual Play candidate; stale targets cannot take focus. OK/Down uses the existing Sources sheet.
- Personal resume/remaining/ends-at line; ends-at also available in Ready. Unknown watch history is never called unwatched.
- Episode title/synopsis concealment extends existing blur; hold reveals only that episode. Season controls include a real-data premiere reminder and persisted No spoilers switch. Existing season progress mark retained as a line.
- Texture-free, bounded episode grid using TMDB scores, watched strokes, same-episode vertical navigation, best available unwatched episode and Play routing. Existing season graph preserved.
- Cast monograms handle UTF-8; focused cast can show a watched credit already in the catalog, keyed by person ID, with a profile setting. No new filmography worker.
- Related posters use real watched history; nested title Back restores section, column, season, tab and scroll.
- Trakt spoiler flags survive parsing/sorting; first OK reveals, second opens full comment. Episode changes clear episode reveals. Comment rail draws once per frame.
- Empty quotes have zero focus stops, including stale previous-title data; series facts are reachable. Original title, original language and series premiere retained from existing TMDB response.
- Round-2 movie section order, series trailers before related titles, footer quotes/facts/production, inactive section alpha 0.42; approved existing section geometry retained.
- Every new label translated in all 28 sibling tables. `python3 tools/varredura-i18n.py` and `python3 tools/idiomas.py` both rc 0.
- `bash tests/detail21.sh` rc 0 after FAIL-to-PASS iterations. `bash tools/build-local.sh` rc 0, one full build. `bash tests/detail21_shot.sh` rc 0; 1920x1080 native PNGs in `/Volumes/ExternalSSD/tmp/rev/detalhe21-resto-png/`.

Still partial / pending (not claimed complete):

- Parallel rating branch still owns star/editor/Your rating, provider/friend block integration; this branch does not implement or port it. Consequently Personal and Both currently differ by source/status line, not a new rating button.
- Ready chip opens the existing Sources sheet, not a separate compact checked-version island. It describes a candidate, not a verified-ready count. No new source preparation/debounce job, episode source chip, or loading ring was added.
- Settings preview is the existing native scene with three choices; separate thumbnail row, full selected frame and detailed profile/history help do not match every `opcao.png` detail.
- Season progress uses existing line, not a new arc. Future reminder uses real premiere season, without fictitious T6/year. Missing total remaining-hours summary and best-season/pace panel.
- Same-author timeline has no author bibliography source in current extras; preserve existing real franchise card/list. Series Universe and chronology stay absent without curated relationships/dates.
- Personalized recommendation reasons are unavailable; no fake rating-based reason is displayed. Trailer rail preserves platform-safe single native clip behavior; watched-video persistence and distinct extra playback remain pending.
- Facts use actual localized catalog title under Title. The source does not guarantee Brazilian localization, so no false No Brasil label is shipped. Content descriptors beyond real certification remain absent.
- Existing 700ms expo/header-at-140 behavior, staged network request order, 350/70ms image cascade and strict 30-texture/two-screen eviction are not fully implemented; shared extras workers/cache remain. No hardware performance claim.
- Existing hero/provider/comment/footer layouts are visibly different from the full round-2 board; exact mock parity is not established. Native shots are evidence of current behavior, not approval.
- Baseline predates top selector: before movie/series top PNGs represent the common old top; there were no two baseline option states to capture. After PNGs cover all three top modes plus episode/progress/notes/cast/comments/footer and explicit spoiler states.

## 7. Mock limitations

Mock names, scores, friends, comments, chronology, cast history and quotes are fixtures, not app data. No mock facts are shipped. Native captures will use isolated deterministic fixtures through the existing shot harness; they do not prove low-RAM TV performance.
