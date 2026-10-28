# AIOStreams dashboard: measured TV diagnostics

Approval specification and static mockups, not implemented application behavior. Evidence, versioned source links, parser contract and owner-side suggestions: [PESQUISA.md](PESQUISA.md). Preview: [mockups/index.html](mockups/index.html). Numeric fixture: [mockups/data.js](mockups/data.js). Follow `/Volumes/ExternalSSD/tmp/rev/DESIGN.md` and existing 2.0.8 components. No push, deployment or remote configuration writes.

## Product decision

Answer from the couch: can I watch, where did waiting happen, what needs attention? Use existing AIO overview and Sources inspectors. Statistics now arrive with ordinary stream results; no separate Search request for every title, server analytics dependency, continuous polling or new telemetry service.

Latest episode: **Streams available**. Host responds in 467 ms; four observed source/service rows report SUCCESS; 20 playable candidates. Wrapper cache coverage is low: AllDebrid 8/503 (1.6%), Real-Debrid 18/503 (3.6%), with one unclassified input each. That is a coverage observation, not a broken debrid account or a failing host. All 248 catalogs enabled; dashboard currently stores only 128, so show **120 omitted from this inventory** until paging is fixed. Manual configure export is known; API config access remains restricted.

## Shared TV contract

Viewer sits in a dim living room, holding a remote; use the existing dark island material, flat rows and generous type. No independent web dashboard shell, score, nested cards, decorative panel outlines or raw upstream emoji.

| 2.0.8 component | Required use |
|---|---|
| `plrui_material` | Same material as clock island: glass rgba(14,15,18,.80), solid #121316; 28 px sheet radius; 24 px outer margin; short shadow, restrained corner light. Same geometry in both modes. |
| `plrui_linha_foco` | Neutral brighter row, 20–22 px radius; no outline or full-accent row. |
| `plrui_seg` | Overview / Add-ons / Pipeline / Configuration; section selection distinct from focus. Sources remains its existing sheet. |
| Existing button/chip helpers | 56 px pill; resting white 8%, focused theme accent and contrast tint. Accent from `ajustes_acento()` helpers, never fixed in application code. |
| `plrui_dicas` | Working OK and Back hints only; clock island owns global activity. |
| Existing `TXT_*` | Inter Display 40/Bold title, 32–36 verdict, 24–28 row, 19 supporting copy; 15–16 only timestamps/footer. Five visible primary rows maximum. |

Lucide only via existing `aj_` assets: activity, server, puzzle, cloud, timer, circle-check, triangle-alert, circle-x, wifi-off, list-filter, sliders-horizontal, refresh-cw, smartphone, chevron-right, arrow-left, clock-3, circle-help. State always has words plus icon, never color alone. No phone-only QR containing installation secrets. Mockups use an illustrative user accent through a CSS variable; implementation reads the actual setting.

All product copy, states, singular/plural, percentages, units, duration formatting, action labels and accessibility labels use i18n. Add keys in byte order to `idioma_tab.h`, synchronize all 28 languages. Public provider brands are sanitized data. Unknown source names become neutral local aliases. Upstream reason enums map to local translated strings; raw name/description/Details never reach UI. Preview English strings form a prototype dictionary, not production localization. Verify 85%/100% scaling and long translations on target devices before implementation acceptance.

Remote: Left/Right within section selector, Down into saved first content row; Up returns to selector. Content lists use Up/Down; OK opens focused detail; Back restores originating section/row/scroll, then exits to Add-ons or Sources. No pointer, color key or long-press requirement. Preserve logical focus across response updates; freeze ranking while a row is focused. Refresh disabled during same-scope request and Retry-After; Back always available. Empty/failing views focus their first enabled action. Profile/installation/variant switch clears foreign history and focus identities. Device implementation retains existing cancellation and request ownership.

## Evidence, local history and verdicts

Classify `streamData.type=statistic/error` before external/playable dispatch. Statistics have no `url` but do carry `externalUrl`; exclude them from Sources, handoff, autoplay, download, result-limit counts and per-quality lists. Metadata-free fallback requires known heading plus grammar and no playback target; missing URL/emoji alone is insufficient. Exact upstream formats, optional clauses and safe parsing are in PESQUISA.md.

One bounded local ring: last **N=10 completed requests** per installation/profile/effective variant/content type. Reuse existing scoped stats. Record request timestamp, client elapsed, response state, safe source/service identities, status, reported count/time, cache coverage, removal enums and phase timings. No media IDs, stream URLs, hashes, tokens or raw Details retained in diagnostic history. Clear with profile removal/reset. No promised durable history until existing storage lifecycle is verified; memory-only is sufficient initially and restarts must state history reset.

History updates only when a request completes, including failed attempts with explicit missing metrics. Missing source reports are gaps, not zero or success. Graph latest measured points among last 10 attempts; label actual count. Keep movie/episode/anime separate; do not compare mixed workloads as a latency trend. Coordinator earlier samples plus new requests provide **two episode observations**, not ten. Preview trend is previous/current 116/95 ms Comet, 228/360 ms Torrentio, 1320/194 ms Debridio AD, 2570/226 ms Debridio RD. Show exact values and sample count; no claim of statistically established improvement or cache warmth.

Verdicts use independent host / add-on / debrid axes:

| Axis | Healthy | Degraded | Failing | Offline / unknown |
|---|---|---|---|---|
| Host | Valid health/manifest with measured latency | At least two of last three comparable controls exceed proposed 2 s TTFB | HTTP 5xx, invalid payload or explicit host failure | DNS/TLS/connect/deadline: cannot reach; no sample: unknown |
| Add-ons | Observed source replies succeeded; zero-result SUCCESS says responded, not usable | Named timeout/error/partial success while other candidates remain; search >5 s says slow even without source blame | All observed sources error and no usable candidates | Missing statistics unknown; loss of host connection leaves old observations stale |
| Debrid | Service checks complete and available cached candidates known separately | Low cache coverage informative; partial service error while alternatives remain | Explicit service/account error and no available alternative | No service report or incomplete/error counts unknown; never infer account failure from low ratio |

Overview state follows usable candidates and confirmed errors, not worst decorative chip. Config 401 is a separate lock; it does not make host/playback failing. Valid empty result with no errors is “No streams for this title,” with filter/another-title action. HTTP 429 preserves facts and adds retry deadline. One failed control is a connection problem, not proof server is down. A source timeout needs structured or recognized named error, not just long duration or a configured 30 s ceiling. Current samples contain no errors; timeout/error states below are explicitly synthetic approval scenarios.

## Screen 1: Overview

Header “AIOStreams”, version Stable 2.37.0, workload, timestamp and one “Check now” chip. One verdict and one supporting line: “20 streams available · latest episode 2.01 s”. Three flat rows open detail:

- Host: “Responding · 467 ms”; health control, not media throughput.
- Add-ons: “4 observed replies succeeded”; raw source counts stay in detail.
- Debrid cache: “AD 1.6% · RD 3.6% of wrapper inputs”; low coverage, not account failure.

Then a compact pipeline bar: Fetch 597 ms / Service wrap 549 ms / other serial work ~64 ms, total ~1.21 s. See Screen 3 for overlap rules. Catalog warning “248 catalogs enabled · 128 shown here · 120 omitted” opens configuration. Primary action “Back to Sources”; secondary “Configure on phone” uses existing safe handoff, not a displayed owner URL. Config snapshot provenance belongs in supporting details, never a false connected badge.

## Screen 2: Add-ons and errors

At most five flat rows, one per source **and service**; heading “Latest episode · reported by AIOStreams · 2 local observations”. Columns readable at TV distance: name/service, explicit status, current duration, raw source stream count, previous/current trend. Stable attention-first order, applied only on explicit refresh/return.

| Add-on / service | Current status | Time | Reported entries | Previous time |
|---|---|---:|---:|---:|
| Debridio Scraper / RD | Responded (SUCCESS) | 226 ms | 1,037 | 2.57 s |
| Debridio Scraper / AD | Responded (SUCCESS) | 194 ms | 1,037 | 1.32 s |
| Torrentio / p2p | Responded (SUCCESS) | 360 ms | 98 | 228 ms |
| Comet / p2p | Responded (SUCCESS) | 95 ms | 924 | 116 ms |

Counts are pre-filter source entries, can overlap and can include errors in partial replies; never sum into 20 final playable candidates. Anime has only Torrentio observed; other sources say “Not reported for this request.” OpenSub Pro/configured metadata sources are not failed merely because no stream scrape summary names them.

OK opens same island detail with current/previous time, sample count, source count, status, owner timeout snapshot (30 s Torrentio/Comet, 7 s Debridio), timestamp and safe error category. Attribute `streamData.error` using safe structured add-on identity or recognized named evidence. Timeout, authentication, rate limit and upstream unavailable get distinct translated copy. Ambiguous evidence says “Source not identified”; never guess from bingeGroup or formatter branding. A PARTIAL SUCCESS row opens both successes and parsed error categories. A thrown failure may have an error entry without a summary; merge without inventing time/count. Error details never display raw URLs, IDs or scripts.

Actions: Back, Configure on phone; configuration changes are suggestions only. No add-on disable switch on this screen.

## Screen 3: Pipeline, removals and debrid cache

Two subviews within the existing inspector, selected with normal remote controls; keep five primary rows visible and page detail.

Pipeline view: server total ~1.21 s beside client total 2.01 s, each labeled. Total-length bar has Fetch 597 ms, Service wrap 549 ms, approximate residual 64 ms. Below it, separately labeled overlapping measures: Media info 220 ms, Filter 57 ms (2 calls), Dedup 9 ms, Precompute 3 ms, Sort 34 ms, Limit 0 ms, SEL 0 ms. Do **not** stack these all together: Media Info is nested; Filter/Precompute accumulate fetcher work. Nested filter detail: expressions 2 ms, regex test 6 ms, filter pass 33 ms, other measured subphases zero. Zero is a reported rounded value, not absence. Source fetches/service checks are parallel; their sums are not wall time. Missing or inconsistent total gives an unsegmented duration bar and “Breakdown unavailable”. No exact network-overhead label from client minus server.

Removal view uses translated reason and count: latest episode Size 58, Excluded visual tag 10 (3D), Excluded keywords 3. Movie example: Size 222, Visual tag 24, Keywords 27, Quality 6 (2 SCR/4 TS). Keep repeated Removal Reasons entries, combine only identical known categories within one request. Never show unsafe keyword strings. Counts are upstream removal events; overlapping/repeated passes mean no guaranteed unique rejected total. Dedup, sorting and final result limiting are separate from removal reasons; 1,037 source entries minus 20 final results is not a valid removed count.

Debrid detail:

| Service | Cached / input | Uncached | Unclassified | Magnet check |
|---|---:|---:|---:|---:|
| AllDebrid | 8 / 503 (1.6%) | 494 | 1 | 344 ms |
| Real-Debrid | 18 / 503 (3.6%) | 484 | 1 | 333 ms |

Caption “Wrapper input checks only; direct debrid results excluded.” Percent uses cached/input; always label denominator. Optional classified-only ratio must also show coverage. Do not pool services as unique torrents, confuse low ratio with service failure, or derive final Sources cache badges from these ratios. Zero-count clauses may be omitted by upstream; `Error` makes counts unavailable. Missing unclassified counts are derived only when input/cached/uncached all parse consistently. OK service row shows processing and service total if reported; no media URL is opened.

## Screen 4: Configuration and suggestions

Five visible groups with scroll/detail: catalog inventory; source timeouts; fetching/groups; playback/preload/formatter; diagnostics/access. Show provenance “Owner configure export” and snapshot age. No API refresh may replace a known count with zero after 401. Missing effective variant context is explicit.

Catalog group warns 248 advertised/enabled, 128 current dashboard capacity, 120 omitted. Remote catalog enablement and local inventory truncation are different. Offer local paging/show-hide once implemented; phone guide for reducing remote catalogs. Reducing catalogs targets Home memory/fan-out, not proven title scrape latency.

Phone-only suggestions: trial 7–10 s Torrentio/Comet ceilings, dynamic fetch early exit, priority groups, useful stream-resource selection, debrid/cache preference and optional stream caching where allowed. Explain late-result/coverage/staleness tradeoffs from PESQUISA.md. Preload/precache are already on; do not propose turning them on as a new fix. Preserve tamtaro and bingeGroup autoplay. “Configure on phone” follows existing protected handoff; static preview has no live destination or settings mutation. Health Checks are expression inputs configured by owner, not a public client diagnostics API.

## Existing Sources sheet

Keep playable results first, no diagnostic rows among them. Latest episode shows **20 sources**, never 31. Only supported playable targets pass to selection/player; error/statistic externalUrl does not become an external source. Retain structured final-candidate cache/quality/service, headers and bingeGroup. Preview source rows use generic redacted names, not copied filenames/URLs; they illustrate geometry only.

Optional compact chip “20 available · 4 add-ons responded” opens Overview with latest workload scope; degraded version “20 available · 1 source issue” keeps selection working. Loading chip stays “Checking sources”; no timing claim before completion. Cache unknown gets no invented cached badge. Refresh must not reorder selected stream or auto-play a diagnostic entry. No extra poll/Search call to populate chip.

## Every screen: required state matrix

These states apply to Overview, Add-ons, Pipeline, Configuration and Sources, including their detail sheets. Never replace cached observations with fabricated zeros.

| Screen | Loading | Healthy | Degraded | Failing | Offline |
|---|---|---|---|---|---|
| Overview | Stable shell, prior check labeled; Check disabled | Actual streams and three-axis facts | Streams usable plus named issue/slow search | Server responded, unusable data or fatal error; targeted fix | Cannot reach; old facts visibly previous; retry |
| Add-ons | Previous rows or “Waiting for diagnostics” | Observed replies and measured trends | Named partial/error rows first; other sources usable | All observed replies error, or invalid diagnostics distinctly labeled | Cached rows stale; no current success claim |
| Pipeline | Previous bar dimmed and labeled | Measured bar, ratios and removal groups | Partial/missing phase/service evidence; unknown gaps | Invalid response/error; no invented totals | Old timing/ratios labeled previous; retry in overview |
| Configuration | Existing snapshot with provenance | Known snapshot, inventory and optional API capability | Restricted API/truncated inventory; independent of playback | Explicit malformed configuration response; retain last valid snapshot | Snapshot still readable as previous; no remote edit |
| Sources | Previous choice stable, pending chip | Only playable rows | Available candidates selectable, health chip names issue count | No playable candidates due to explicit error; inspect health | Connection failure, prior sources marked previous/unverified |

Valid empty search is a separate non-outage state with filters/another-title guidance. Missing statistics is capability unavailable, not failing add-ons. Rate limit is a deadline overlay retaining prior data, not an automatic retry loop. Synthetic failure/offline states in mockups are labeled “Scenario”; measured healthy data is labeled “Observation”.

## Approval artifacts and acceptance

One dependency-free static HTML preview with local Inter Display/Lucide assets and redacted numeric data; no owner endpoint/network calls. Query `?screen=overview|addons|pipeline|configuration|sources&state=loading|healthy|degraded|failing|offline&material=glass|solid`. Reference viewport 1920×1080. Arrow keys navigate focus/sections; Enter opens detail/navigation; Escape returns. Preview “Check now” does not call the service; it explains the preview boundary. All 25 screen/state combinations render; PNG exports cover the five main screens, four overview states and solid overview. PNGs live externally in `/Volumes/ExternalSSD/tmp/rev/aiopesq/`; HTML/data/renderer and docs are committed.

Acceptance check: `bash tests/aiostreams_round2_mockups.sh` validates numeric fixture invariants, secret-free allowlists, all screen states, no browser overflow, D-pad/Back behavior, local asset loading, and emits approval PNGs. Existing native build: `bash tools/build-local.sh`. Mockup is not proof of implemented C parser, device performance, first frame, subtitle flow or next-episode correctness. Those remain implementation jobs 1–8 in PESQUISA.md; owner approval of these artifacts remains pending.
