# AIOStreams integration research

Scope: research, read-only probes, TV design and a reusable probe. No application changes. Measurements: 2026-10-28 UTC, coordinator's Mac workspace; do not extrapolate these numbers to TV networking or decoding. Owner endpoints are called `<owner-manifest>` and `<owner-manager>` throughout. Raw bodies and response headers stay only in private external storage.

## Findings that change the implementation

1. **Configuration access is broken for this installation, while streams and Search API work.** `GET /api/v1/user` and `/api/v1/user/analytics` return 401 using installation credentials. v2.37.0 code explicitly rejects encrypted passwords for these routes. Search accepts them. The User API documentation still says encrypted passwords work: code and live behavior take precedence. Never classify this as an offline host or failed installation. Do not obtain a raw password by decoding the installation token. [S1–S4]
2. **Production stream requests miss structured metadata.** Normal Nuvio networking uses `Nuvio/1.0 (webOS)`; dashboard requests use `AIO-NuvioLegacy`. Default upstream policy exposes `streamData` only to User-Agents starting with `AIO`. A control with Nuvio's normal agent returned 20 streams, zero `streamData` objects and 19,721 bytes; diagnostic agent returned 20 structured streams and 60,928 bytes for the same episode. Consistent client identity is a correctness fix for provider/cache/quality attribution. [S5]
3. **Search is slow before the first byte; named fault attribution is unavailable.** Movie TTFB 1.487–8.174 s, series 2.511–5.910 s, anime 1.161–2.751 s. Health controls 0.312–0.500 s. Every measured stream response contained 20 results and no detected error streams. Search returned zero errors and zero statistics. This supports search-path processing/waiting, not a proven broken add-on or persistent host outage. [S6–S9]
4. **Dashboard silently caps the owner's 248 catalogs at 128.** `AIO_MAX` and `aio_manifesto()` stop enumeration at 128 without reporting an omitted total. At least 120 entries cannot appear in that dashboard inventory. This finding concerns dashboard inventory; it does not establish the same limit in Home's independent discovery path.
5. **Do not add a second integration stack.** Setup, inspectors, resolved configuration traversal, stream metadata parsing, parallel add-on queries, manifest cache, partial publication and early automatic selection already exist. Fix capability handling, reuse existing network measurements and scope/cancellation, then simplify status presentation. AIOManager is a separate management product with separate authentication; its URL is not an AIOStreams credential. [S15–S16]

## Sources and version boundary

AIOStreams pinned to release `v2.37.0`, commit `ed8b085f081bc8442dc59ac9de4a4a2070b93f0a`. The public wiki now redirects readers to the documentation site. Versioned source includes the docs, so the following links remain reproducible. AIOManager inspected at `c1b95c51f54f47f1090ff8fba901414b3566602d`; compatibility with a particular hosted manager version is capability-dependent.

- **S1:** [User API documentation](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/docs/apis/user.mdx), [wiki API redirect](https://github.com/Viren070/AIOStreams/wiki/API-Documentation).
- **S2:** [User routes: GET user, analytics, credential requirements](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/routes/api/user.ts#L110).
- **S3:** [Basic authentication and encrypted-token rejection](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/utils/basic-auth.ts#L35).
- **S4:** [Search route and authentication](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/routes/api/search.ts), [Search documentation](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/docs/apis/search.mdx).
- **S5:** [Stremio route / User-Agent metadata policy](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/routes/stremio/stream.ts#L42), [Stremio transformer](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/transformers/stremio.ts).
- **S6:** [Health: database ping](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/routes/api/health.ts), [public status/capabilities](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/routes/api/status.ts).
- **S7:** [Fetcher: timing, per-source errors, grouping and early exit](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/streams/fetcher.ts), [statistics gating](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/main/resources.ts#L760).
- **S8:** [Pipeline timing/filter/service statistics](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/main/statistics.ts), [Search result transformer](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/transformers/api.ts).
- **S9:** [Resource cache and timeout wrapper](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/main/wrapper.ts), [resource defaults](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/config/schema/resources.ts).
- **S10:** [Add-on/config/catalog/proxy schemas](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/db/schemas.ts), [configure options](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/docs/configuration/options.mdx).
- **S11:** [Rate-limit implementation](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/server/src/middlewares/ratelimit.ts), [default limits](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/config/schema/rate-limits.ts), [default add-on timeout](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/config/schema/presets.ts#L128).
- **S12:** [Groups guide](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/docs/guides/groups.mdx), [manifest initialization](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/core/src/main/setup.ts#L235).
- **S13:** [v2.30 migration](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/docs/migrations/v2.30.mdx), [v2.32](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/changelog/v2.32.mdx), [v2.33](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/changelog/v2.33.mdx).
- **S14:** [v2.35](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/changelog/v2.35.mdx), [v2.36](https://github.com/Viren070/AIOStreams/blob/v2.37.0/packages/docs/content/changelog/v2.36.mdx), [v2.37.0 release](https://github.com/Viren070/AIOStreams/releases/tag/v2.37.0).
- **S15:** [AIOManager overview](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/README.md), [API authentication](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/content/docs/api/index.mdx), [endpoints](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/content/docs/api/endpoints.mdx).
- **S16:** [AIOManager security](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/content/docs/reference/security.mdx), [server routes and health](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/server/index.js), [OpenAPI](https://github.com/Sonicx161/AIOManager/blob/c1b95c51f54f47f1090ff8fba901414b3566602d/server/openapi.json).

Release implications through 2.37: 2.30 introduced runtime settings/admin analytics; 2.32 expanded formatter/search and configuration profiles; 2.33 introduced variants; 2.35 added AIO's app/media-server surfaces and catalog editor changes; 2.36 expanded TV clients. 2.37 adds watch-party/SyncPlay, playlists and client hints, and fixes merged catalog extra handling. These are not a requirement to replace Nuvio's player or implement Jellyfin/party APIs. Variants and client hints mean the User-Agent and exact manifest route/query can affect effective configuration. Do not assume `/user` always describes the variant used by a specific stream request. [S13–S14]

## Public HTTP contract useful to a TV client

| Endpoint | Authentication / meaning | Client policy |
|---|---|---|
| Personalized `manifest.json`, `/stream/{type}/{id}.json`, `/meta`, `/catalog`, `/subtitles` | Installation route includes credentials or alias/variant; standard Stremio JSON | Preserve full installation base/query; GET only; no URL in diagnostics |
| `GET /api/v1/health` | Public DB ping; success envelope | Reachability control, not proof every source works |
| `GET /api/v1/status` | Public version/channel/settings/presets/services/limits | Cache capability summary; response can include credential-like encrypted settings; allowlist output |
| `GET /api/v1/user` | Raw Basic password or config session in 2.37 code | Optional resolved config; `?raw=true` omits inheritance resolution; 401 means config unavailable |
| `GET /api/v1/search?type=...&id=...&format=true` | Basic credentials, including encrypted install password, or `x-aiostreams-user-data` | Optional diagnostics; returns results/statistics/errors; avoid running alongside identical normal stream query |
| `GET /api/v1/user/analytics?range=24h` or `7d` | Raw config credentials/session; instance and per-user analytics must be enabled | Optional history only; do not probe admin routes or demand admin access |
| `GET /api/v1/anime` | Public mapping API; documented identifier parameters | Optional ID mapping, never required for the measured direct Kitsu route |

No independent public `/api/v1/statistics` route was established. Search statistics are embedded in the response; aggregate admin dashboard metrics require a different privilege boundary. No `/api/v1/config` assumption: the user configuration is `/user`. POST/PATCH/DELETE configuration, session creation, password changes and health-test actions are outside this task. [S1–S8]

`format=true` adds user-formatted `name`/`description` to Search API results; the envelope remains `data.results`. `data.filtered` counts missing `requiredFields`, not all pipeline filter removals. Do not label it “Nuvio hidden” or “all upstream rejected.” Statistics/errors are human strings with `title` and `description`, not stable numeric timing objects. [S4, S8]

### Stream semantics

- `name`/`description` may contain arbitrary templates, passthrough text, emoji, multiline metadata and even secrets. Never use their layout as the primary protocol. Preserve structured unknowns rather than inferring success/cache from a decorative character.
- `behaviorHints` includes `bingeGroup`, `filename`, `videoSize`, `videoHash`, `notWebReady`, `proxyHeaders.request/response`, plus inherited hints. `bingeGroup` describes autoplay equivalence, not add-on identity. `videoSize` is bytes and may exceed 32-bit range. Filename supports language/quality fallback; headers must reach the player without entering diagnostics.
- Optional `streamData` includes `type`, `addon`, `service`, `parsedFile`, `filename`, `size`, `folderSize`, `torrent`, `proxied`, `indexer`, duration and matching details. Cache comes from `service.cached` where present; absence means unknown. Metadata exposure can be globally disabled, forced, or IP-restricted even with an AIO agent.
- Upstream errors are usually **HTTP 200 informational entries**: `externalUrl` plus `streamData.type="error"` and `streamData.error.{title,description}`. Statistics use `type="statistic"`. Neither is a playable candidate nor an arbitrary external handoff. Failure descriptions name an add-on and may say a resource timed out after a number of milliseconds. `hideErrors`/`hideErrorsForResources` can suppress entries; a clean Stremio response alone is not proof all sources succeeded. Search's `errors` provides additional evidence.
- Supported output forms also include HTTP/debrid, P2P `infoHash`/`fileIdx`/`sources`, YouTube, external links, NZB/server data and archive URL arrays. Existing parser/playback support must be checked per form; protocol presence is not a promise of TV playback. No media URLs were opened here. [S5, S7–S10]

### Timing, caching, quotas and proxy

Default add-on timeout is 7,000 ms; per-add-on options override it within instance limits. Manifest initialization fetches source manifests concurrently, with 3,000 ms normal / 10,000 ms increased defaults. Static groups await parallel requests; sequential groups avoid starting later sources, while parallel groups can exit early and discard later results. Dynamic conditions can stop waiting for unnecessary work. “Parallel” reduces wall time, not request count. [S7, S9, S11–S12]

With `statistics.enabled` and `statsToShow` containing `addon`, successful source scrape summaries carry source name, count, status and textual elapsed time. `timing` adds pipeline fetch/filter/dedup/sort/service-wrap breakdowns. Numeric latency dispositions also exist internally but are not exported as a stable numeric field in the public Search envelope. Parse known statistic formats conservatively; keep unrecognized evidence unavailable. Owner returned no statistics, and per-user analytics is disabled in public status. [S7–S8]

Defaults: manifest cache 21,600 s, manifest-failure cache 60 s, subtitle cache 300 s, per-source stream cache disabled (`-1`), full-pipeline cache disabled (`0`). Host settings can override these; they were not established for the owner. Cache hits can avoid refetching sources; whole-pipeline caching also preserves partial errors until expiry. Background requests may continue after foreground timeout to warm cache; they need concurrency limits. Do not infer “cold” and “warm” with certainty from repeated timings alone. [S9]

Limits are instance-defined and per IP. Upstream defaults include User API 5/5 s, Search API 5/10 s, plus independent Stremio resource buckets. Respect 429, `Retry-After` and standard rate headers; never continuously retry a failing configuration. This probe stops on 429 instead of sleeping/retrying. [S11]

MediaFlow/StremThru proxy settings wrap media and selected services; a proxy may add playback latency or allow required headers/IP affinity. `streamData.proxied` indicates wrapping, not proxy health. Search latency cannot establish media download throughput or first-frame time. Do not turn the proxy off automatically: it may be essential for access. [S5, S10]

Catalog modifications support name/order/enablement, shuffle/reverse, discover/search/home flags, type override and poster service. Merged catalogs combine source lists with ordering/deduplication. A manifest advertises catalog descriptors; fetching it does not fetch 248 catalog pages. Initialization can still wait for underlying source manifests. Reducing catalogs helps Home fan-out and TV memory; disabling unused source resources/add-ons can also reduce initialization. Attribute these separately. [S10, S12]

Password boundaries: installation encrypted password grants stream/search access, not necessarily configuration read access. Global instance protection (`addonPassword`), config password, config sessions, aliases, variants and AIOManager master/sync credentials are distinct. Preserve manifest-only operation for opaque aliases. Never try all permutations of IDs or decode/decrypt installation secrets on the client. [S1–S4]

## Measurements

29 owner-service requests total: 5 manifests + 15 streams + health/status/user/search + 4 controls + 1 manager health. No redirects followed, retries, configuration writes, playback checks or catalog-page fan-out. Five sequential repeats per workload, fresh curl process per request. No deliberate cache busting; diagnostic User-Agent starts with `AIO`. Public research downloads are excluded from the owner-service budget.

IDs: movie `tt0133093` (The Matrix), series `tt0944947:1:1` (Game of Thrones S1E1), anime `kitsu:1:1` (Cowboy Bebop E1), requested as `movie`, `series`, `anime` respectively. One successful anime does not prove all ID/type combinations.

All times below are seconds. DNS is curl `time_namelookup`; TLS is `time_appconnect - time_connect`, floored at zero; TTFB/total measured from request start. Size is downloaded bytes. Stream counts count response entries; Search uses `data.results`. Error column covers detected informational error entries or API authentication result, not HTTP alone.

| Request | DNS | TLS | TTFB | Total | Bytes | Streams | HTTP / errors |
|---|---:|---:|---:|---:|---:|---:|---|
| Manifest 1 | .158888 | .038716 | .530079 | .560428 | 74345 | — | 200 / none |
| Manifest 2 | .002350 | .053031 | .448284 | .479523 | 74345 | — | 200 / none |
| Manifest 3 | .001790 | .037498 | .428866 | .504839 | 74345 | — | 200 / none |
| Manifest 4 | .002142 | .041921 | .386241 | .418808 | 74345 | — | 200 / none |
| Manifest 5 | .002228 | .035522 | .351467 | .378444 | 74345 | — | 200 / none |
| Movie 1 | .002377 | .033437 | 6.187983 | 6.221589 | 70107 | 20 | 200 / none |
| Movie 2 | .002445 | .038889 | 2.834347 | 2.851567 | 70107 | 20 | 200 / none |
| Movie 3 | .001920 | .079402 | 8.173923 | 8.216388 | 70107 | 20 | 200 / none |
| Movie 4 | .002163 | .053733 | 1.487339 | 1.494428 | 70107 | 20 | 200 / none |
| Movie 5 | .001815 | .039352 | 1.886497 | 1.917364 | 70107 | 20 | 200 / none |
| Series 1 | .002022 | .034336 | 5.910088 | 5.922771 | 60928 | 20 | 200 / none |
| Series 2 | .002554 | .090888 | 4.817607 | 4.841197 | 60928 | 20 | 200 / none |
| Series 3 | .002074 | .100506 | 4.329484 | 4.358448 | 60928 | 20 | 200 / none |
| Series 4 | .002479 | .095561 | 2.958842 | 2.987207 | 60928 | 20 | 200 / none |
| Series 5 | .002260 | .102235 | 2.511493 | 2.539207 | 60928 | 20 | 200 / none |
| Anime 1 | .001939 | .040791 | 1.806455 | 1.870989 | 76315 | 20 | 200 / none |
| Anime 2 | .001911 | .092696 | 1.895235 | 1.980101 | 76315 | 20 | 200 / none |
| Anime 3 | .002058 | .158877 | 2.751088 | 2.832985 | 76315 | 20 | 200 / none |
| Anime 4 | .002364 | .078582 | 1.160597 | 1.232159 | 76315 | 20 | 200 / none |
| Anime 5 | .002127 | .081936 | 2.245711 | 2.326541 | 76315 | 20 | 200 / none |
| Health | .002406 | .084178 | .499579 | .499778 | 55 | — | 200 / none |
| Status | .002060 | .090548 | .500747 | .763504 | 403244 | — | 200 / none |
| User | .002083 | .096450 | .477426 | .477679 | 150 | — | 401 / unauthorized |
| Search, series | .002109 | .066560 | 2.852385 | 2.882265 | 63496 | 20 | 200 / no errors, no stats |
| Series, native agent | .120436 | .062983 | 5.110230 | 5.117890 | 19721 | 20 | 200 / metadata absent |
| Analytics | .002564 | .042940 | .350226 | .350465 | 150 | — | 401 / unauthorized |
| Health control 1 | .002147 | .036989 | .364068 | .364271 | 55 | — | 200 / none |
| Health control 2 | .002085 | .035827 | .312184 | .312483 | 55 | — | 200 / none |
| Manager health | .244996 | .071366 | .522911 | .523124 | 187 | — | 200 / status ok |

Manifest confirms Stable 2.37.0, resources stream/meta/catalog/subtitles, types movie/series/anime/collections, 248 catalogs. Movie median TTFB 2.834 s; series 4.329 s; anime 1.895 s; manifest .429 s. First vs repeated is not monotonically faster: movie repeat 3 was slowest. No 429 or numeric Retry-After observed in the main run. Bodies were stable in size within each primary workload, not necessarily identical in content.

Returned source labels recognize Comet, Debridio and Torrentio for movie/series; Torrentio for this anime. These are contributors, **not a complete enabled-add-on inventory**, because only 20 ranked results survive and configuration read is unavailable. No named source failure or timeout was observed. Hidden Stremio errors remain possible, though the one Search request also reported no errors. No claim of per-source latency is supported.

Coordinator's earlier wired measurements were manifest TTFB .65–3.4 s and one series request about 8 s for 20 streams. Current manifest run is faster; both observations support variability. They are different sessions and are not a controlled before/after experiment.

## Diagnosis and remedies

| Observation | Supported diagnosis | Next useful action |
|---|---|---|
| DNS usually ~2 ms; TLS ~33–159 ms; stream body transfer ~7–85 ms | Multi-second delay mainly precedes body delivery | Compare search evidence with health control, preserve TTFB vs total |
| Health consistently .312–.500 s; movie sometimes 8.174 s | Search-path wait/CPU/metadata/service work exceeds basic host response | Enable add-on + timing statistics manually on AIO, then retest one title |
| No source timings or configuration access | Cannot name a slow add-on or inspect effective timeouts/cache | Show “Search slow; source timing unavailable”; offer phone configure |
| 248 catalogs, 74 KB manifest | Large inventory; not proof of stream latency cause | Lazy-load visible Home rows; separately reduce unused remote catalogs |
| User 401 with Search 200 | Configuration authorization boundary | Keep playback/search usable; show optional config access unavailable |

Owner-side recommendations, **not applied**: enable statistics `addon` and `timing` for a short diagnostic session; compare providers on the same title. Put reliable providers in the first group, use sequential fallback to reduce requests or parallel early exit when wall time matters. Review a trial 3–5 s budget only for a provider repeatedly proven slow; retain larger budgets for indispensable sparse/anime sources. Current configured values are unknown. Remove unused/duplicate providers and disable resources they need not serve. Trim remote catalog rows to those actually used. Check manifest-failure cache and shared cache health; trial a short per-source stream TTL such as 60–120 s only where signed-link lifetime/IP semantics permit. A short pipeline cache can help repeated searches but will preserve failures; avoid treating it as a cure for unreliable sources. Investigate host capacity/DB/network if health also degrades. Use the host dashboard for logs, without exporting secrets into Nuvio.

Client-side recommendations: reuse `desc_manifesto_cache_*`, `fontecache`, account/profile generations and `addonstats`; publish cached inventory immediately then refresh once in background. Start normal streams independently of status/config/subtitles. Do not double-query Search and stream on every inspection; explicitly request diagnostics or reuse returned evidence. Existing parallel add-on and partial-result publication can show other installed providers while AIO waits. AIO's single JSON response itself cannot be progressively exposed. Preserve manual selection during updates and use `fonteauto_pode_decidir()` for existing safe early selection; do not bypass wait-all preference. Keep separate soft “still searching” and hard network deadlines, cancellation and late-response discard. Reuse the existing 30 s VOD request policy first; dashboard's 12 s request timeout should not silently contradict it. Cap prefetch to the next likely episode/title, cancel superseded work and honor rate-limit backoff. Never prefetch every catalog or probe media URLs for health.

## AIOManager boundary

Sonicx161/AIOManager manages add-on libraries, account/device synchronization, providers and Autopilot failover. It is not AIOStreams' configure page and is not part of the Stremio stream response contract. Its public `/api/health` and `/api/config` can provide optional manager reachability/configuration; the measured owner-manager health is OK. `/api/ready` exists in inspected server code. Sync/provider/Autopilot APIs need their own sync password, user ID, derived token or API key; encrypted cloud data is not a ready-to-render config summary. Admin queue/cache statistics are privileged. [S15–S16]

Useful near-term TV action: offer an explicit private phone handoff to `<owner-manager>` if the user stored one; keep its URL masked and out of screenshots/QR logs. Do not infer credentials from its path or fetch sync/account data. A separate opt-in authenticated manager integration would need a versioned capability and key-storage design; it adds no evidence needed for the present AIO playback fixes. No private manager page or authenticated manager API was opened.

## Integration gap analysis

History inspected before design: `10a8edfd4` added full configuration detail; `1edc83d22` persona-lock redaction/focus; `54e539900` script-payload redaction; `76f9d5eb9` history/primary focus; `25d3f8e40` paged remote navigation; `973f0654d` immutable early automatic selection. Existing reviews describe earlier gaps and are superseded only where current code/evidence proves progress.

Severity: P1 blocks a trustworthy integration or core required behavior; P2 materially degrades completeness/UX; P3 optional extension. No P0 outage demonstrated.

| Severity | Location / gap | Evidence / required result |
|---|---|---|
| P1 | `src/aiostreamsui.c:worker`, config auth | Encrypted installation credentials rejected by 2.37 User API; represent config/search/playback capabilities independently; no password workaround |
| P1 | `src/addons.c:baixarStream`, `src/rede.c` default agent | Production agent loses all optional structured metadata in control; use documented AIO client identity on AIO requests, including variants |
| P1 | `src/aiostreams.c:aio_manifesto`, `src/aiostreams.h:AIO_MAX` | 248 catalogs silently reduced to 128; count full inventory and page bounded visible storage or report explicit truncation |
| P1 | `src/aiostreamsui.c:SAUDE` | Fixed check icon and raw HTTP rows do not distinguish offline/host slow/search slow/add-on timeout/config locked; no evidence-based state aggregation |
| P2 | `src/aiostreamsui.c:worker` | Serial manifest/status/user/Search/stream/subtitle requests, 12 s each, one snapshot at end; status/setup work delays title results and duplicates search |
| P2 | `src/aiostreams.c:aio_resultados` | `error`/`statistic` external entries classified as ordinary external links; informational entries need dedicated evidence, not playable/link counts |
| P2 | `src/aiostreams.c:aio_search`, `AioEvidencia` | Only 32 text entries, no parsed provider timing/status, no omitted count; support known scrape/timing syntax and unknown state |
| P2 | `src/aiostreamsui.c`, existing `addonstats` | Dashboard stores total milliseconds but lacks phase history, sample count/freshness and per-source provenance; reuse network measurement primitives |
| P2 | `aio_url`, `aio_autorizacao`, worker URL construction | Only canonical two-component install auth recognized; API origin strips reverse-proxy base path; query/path variants need fixtures and explicit opaque fallback |
| P2 | `aio_resultados`, generic `stream_extrair` | Generic parser already handles filename/videoSize/bingeGroup/headers and structured tracks; verify cache/quality parity under structured and unstructured response, not another parser |
| P2 | Setup/config inventory, `aio_configuracao` | Detail traversal/redaction exists, but unavailable config must not imply zero configured providers/filters; show inheritance/variant uncertainty |
| P2 | Catalog/local controls | Distinguish remote advertised total, remote enablement and local Home visibility; do not promise local hiding fixes remote aggregation |
| P2 | External/NZB/YouTube/archive forms | Inventory is not full actionable playback support; define unsupported handoff and exclude notice/error URLs; acceptance remains per transport |
| P2 | Subtitles | Inventory/timing exist; language flags, per-file/hash matching and capability fallback need end-to-end acceptance; no playback measured here |
| P2 | Secret display/logging | Existing persona/script and sensitive-key redaction is valuable; arbitrary upstream titles/descriptions need strict output allowlists, not substring-only safety |
| P2 | Localization/material/remote parity | All new visible copy via i18n; existing earlier report notes English fallback locales; verify glass/solid, 85/100% scaling, focus restore and clock island on TV |
| P3 | AIOManager API | Optional independent integration; health reachable, authentication contract not established for owner |
| P3 | Jellyfin, parties, admin analytics | Separate products/capabilities, unnecessary for required Stremio integration |

## Small implementation jobs and acceptance

Run jobs in order where dependencies exist. Each changes the narrowest existing layer, with one target regression test and one native build. No new HTTP abstraction, second cache, generic telemetry subsystem or separate UI framework.

1. **P1, capability/auth contract** (`aiostreams.c/.h`, worker). Independent flags for manifest, Search, config and analytics; 401 explains config restriction without disconnecting streams; no raw secret prompt on TV. Acceptance: synthetic manifest 200 + status enabled + user 401 + Search 200 yields playable/diagnostic capability; encrypted vs raw/session cases; aliases remain opaque; account switch drops old snapshot.
2. **P1, metadata/request identity** (existing add-on network caller). AIO agent on identified AIO requests; preserve exact route/query/variant context. Acceptance: local server fixture asserts agent and variant; structured/unstructured outputs produce equivalent playable lists with unknown fields honest; headers, 64-bit size and bingeGroup survive; no media URL fetch.
3. **P1, complete catalog inventory** (AIO snapshot/parser/view). Separate total from visible slice; reuse paging. Acceptance: synthetic 248 and 500 catalog manifests have correct count, last item reachable, stable IDs/focus, bounded allocations, explicit unavailable/truncated state on limits. Verify Home's separate limits without indiscriminately raising them.
4. **P1/P2, honest diagnostics** (AIO parser + existing stats). Classify informational error/statistic entries before external links; known source/time parser, timestamp/sample count and evidence enums; rate backoff. Acceptance: mixed valid/error/statistic/empty/hidden-error payloads, HTTP 200 timeout entry, HTTP 401/429/503, DNS/TLS failure and malformed JSON; unknown timings never become zero; hostile labels/tokenized URLs never render.
5. **P2, responsive requests** (worker + cache reuse). Publish manifest cache first, launch streams independently, request Search only on diagnostics action, defer subtitles and config. Acceptance: slow status/user cannot delay normal results; no duplicate same-title request; cache/profile/variant isolation, cancellation, late result discard, wait-all preference preserved, 429 blocks repeated refresh.
6. **P2, TV overview and source health** (`aiostreamsui.c`, existing components and i18n). Implement DASHBOARD.md using shared island. Acceptance: D-pad reaches every action, Back restores prior focus/scroll; all five states plus config-restricted/unknown; Lucide only; no source-health claims based solely on returned provider labels; glass/solid screenshots and couch readability.
7. **P2, config/catalog/action detail** (existing inspectors/phone handoff). Summarize unavailable data honestly; local vs remote actions labeled; actionable phone guide and recheck. Acceptance: complete synthetic effective config, inheritance/variants warning, unknown new section hidden safely, no mutation from TV, no credentials in QR captures/logs, state returns to previous row after recheck.
8. **P2, device acceptance** (verification only). Run a movie, series episode and Kitsu anime through Sources, selection, first frame, next episode, required headers/proxy and subtitle flow on target TVs. Measure UI frame budget and memory on weak hardware, glass/solid, 85/100% scale, offline and recovered network. “100% working” requires this evidence; Mac curl and a native build cannot establish it. Optional AIOManager work remains outside these jobs.

## Reusable probe

`bash tools/aio-diagnostico.sh --help` describes input. Supply the manifest with `AIO_MANIFEST_URL` in the caller's environment or a file path argument containing one URL or a `manifest=` line. Never place a real URL in a shell command/history. Default run makes 20 GETs; `--api` adds public health/status and canonical-credential user/search probes (maximum 24). `--repeat 1` makes a short pass. No configuration-changing method, redirects, playback requests, retries or insecure TLS. Curl configuration including URL/auth arrives through stdin, not process arguments. Curl stderr and raw text are never echoed.

Optional `AIO_CAPTURE_DIR` must name a new directory beneath the private capture root; files use owner-only permissions. Without it, temporary captures are removed at exit. Stdout contains only numeric measurements, fixed workload names and recognized public provider names/reason categories. Unknown names become `unattributed`; this is deliberate protection against private labels. Search result/error counts are supported. Regex fallback error detection can produce false positives from arbitrary formatted prose; structured `type=error` and Search errors are stronger evidence. HTTP failure is recorded per row rather than aborting the whole series; shell exit success means probe execution completed, not service health.

Checks for this change: `bash tests/aio_diagnostico.sh` (offline mock curl, request budget, structured/API errors, query preservation, 429 stop, transport failure redaction, invalid inputs) and `bash tools/build-local.sh` (existing native verification build). No application code, live configuration, push or deployment changes.
