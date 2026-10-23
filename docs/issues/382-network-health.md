# #382 — Network health while opening a title

Evidence: local plain-text log `log-382-RSM044.txt`, Nuvio 2.0.5 on webOS,
2026-10-13 11:02. No GitHub or device access was used.

## What the log shows

At lines 843–852, the user opens `movie/tt29355505` (Toy Story 5), at app
clock 199403 ms. Metadata, streams, subtitles and extras start together.
This is a movie detail, despite the comment calling it a show.

| Request / host | Result in the log |
| --- | --- |
| Metadata, `aiometadata.stremio.ru` | Final curl 28, 0 bytes, 5073 ms; connect timeout after 1918 ms (874–875). |
| Streams, `aiostreams-nightly.stremio.ru`, both AIOStreams addons | Final curl 28, 0 bytes; first round 7752/7753 ms; addon retry finishes at 15459/15460 ms with no response (883–887, 915–920). Declared addon deadline is 30 s, but connection timeouts end it earlier. |
| Trailer, `tv.apple.com` | Final curl 28, 0 bytes, 8503 ms; DNS resolution times out after 3499 ms (876–877). |
| Providers, `api.themoviedb.org` | Final curl 28, 0 bytes, 8503 ms; DNS resolution times out after 3500 ms (889–890). |
| Trailer, `api.graphql.imdb.com` | Final curl 28, 0 bytes, 10004 ms; DNS resolution times out after 5000 ms (891–893). |
| Subtitles, `submaker.elfhosted.com` | Final curl 28, HTTP 0, 0 bytes, 8874 ms; connect timeout after 4426 ms (894–897). |
| Quotes, `query.wikidata.org` | Final curl 28, 0 bytes, 10011 ms; DNS resolution times out after 5001 ms (906–907). |
| Ratings, POST to `api.mdblist.com` | Repeated curl 28 connect timeouts, approximately 3850–3900 ms (866, 882, 903, 914, 928, 936, 947). |
| Fallback metadata, `catalog.nuvio.tv` | Final curl 28, HTTP 0, 0 bytes, 3583 ms; connect timeout after 1601 ms (924–927). |
| Fallback metadata, `v3-cinemeta.strem.io` | Final curl 28, 0 bytes, 6263 ms; connect timeout after 2404 ms (935–938). |

These are DNS/TCP failures across distinct hosts, not HTTP 4xx/5xx and not
one slow addon. A later image fetch succeeds in 250 ms at line 979, after
the user leaves the detail. The image log does not identify its host.
`conexao reusada` means CURLINFO_NUM_CONNECTS was zero; a connection attempt
that never completed also has zero new connections. That label alone does
not prove a stale reused socket.

## Decision and correction

The i18n key is `Sem internet. Mostrando o que já estava salvo.` in
`src/ilhasinais.c`, function `rede()`. `ilhasinais_passo()` calls it each
frame. `ilhasinais_iniciar()` registers `rede_saude_nota()` as the network
listener. `src/redesaude.c:rede_saude_passo()` changes state after three
consecutive transport failures, at least two hosts and at least three
seconds, with no success between them. An external success clears offline;
local hosts and cancellation are ignored. The same state also controls the
central clock and log-upload availability.

The defect was in `src/rede.c:rede_baixar_interno3()`: releasing the first
failed attempt through `soltarHandleR()` notified network health *before*
the retry finished. Recovered requests could therefore create an offline
transition and a subsequent recovery transition. `soltarHandleR()` also
ignored a received HTTP status when curl later failed on the response body.

The correction discards an intermediate handle without notifying health
or final-request statistics. Only the completed request decides health.
A received HTTP status counts as connectivity even if the body fails;
the original curl error remains available to the caller and host statistics.
Cancellation and deliberate byte limits remain ignored. Existing retries,
connection reuse, specific addon errors and multi-host offline thresholds
remain in place.

RSM044 does not log health transitions or the displayed message, so the
exact request that triggered the UI cannot be identified from this log.
It does prove that intermediate failures were eligible to trigger it, and
that final failures across many hosts would still justify offline under
the existing rule. The change does not claim to fix the underlying TV DNS
or TCP outage, or prove that this whole outage was a false offline result.

## Validation and remaining device work

`tests/rede_saude.sh` exercises real network wrappers and the health state
with fake curl and a fake clock, with no network access:

- Before the fix, both retry recovery and HTTP-response/body-timeout cases
  fail with rc 134 (assertion failures).
- After the fix, both pass with rc 0. Final failures across distinct hosts
  still set offline, and a failed body after HTTP 503 clears it.
- `tests/redesaude.sh`, `tests/rede_tmdb.sh` (native, Android, TPK40),
  `tests/rede_reuso.sh` and `tests/rede_parada.sh` pass with rc 0. The last
  two use only a loopback HTTP server.

On the affected LG, verify cold boot and opening a title with a recovered
retry, a single unavailable addon, and actual internet loss/restoration.
Capture DNS/TCP behavior during a recurrence to distinguish TV resolver,
router/link and app transport problems. No TV build, push or deployment
was performed.

## KE4F4Z — cold boot, 2.0.5 release build (2026-10-14 16:37)

Log `log-382-KE4F4Z.txt`, LG 50UT8050PSB, webOS 10.3.2. The log has no
`[rede] libcurl=` line and no `[rede] saude=` transition line, so neither the
curl/c-ares build nor a health transition is visible in it.

Timeline (app clock, ms from boot; the first log lines have no timestamp):

- 0–5200: Home painted from disk cache; first image fetches fail with
  `curl 28 ... conexao reusada, 0 bytes, 4004 ms` (images.metahub.space).
- ~5–20000: `Resolving timed out after 5001/2001/3499/5000 ms` across
  api.nuvio.tv, postersplus.slokker.cc, aiometadata/aiostreams.stremio.ru,
  submaker.elfhosted.com, api.themoviedb.org, api.github.com,
  raw.githubusercontent.com, api.simkl.com, images.metahub.space — 19 failures,
  all DNS, 9+ distinct hosts. No connect/TLS failures, no HTTP errors.
- 20468: `catalogo da rede publicado` — requests succeed from here on
  (catalog, simkl `plantowatch`). So the resolver recovered about 20 s after
  boot, not an outage.
- 22950 onward: Settings open; three `POST nuvio-recomendacoes.../v1/registro`
  (log upload) fail with `Resolving timed out after 5001 ms`, ending
  `[avisos] registro enviado: HTTP 0` (status 0 → "connection dropped
  mid-upload"). The resolver is again slow for that one host (workers.dev, not
  cached by the TV resolver, new name for this session) while other hosts work.
  The upload made a single attempt and no retry.

Would the reland fix (046fe5f5/1656ce21) have prevented the offline message?
No, not fully. Each of the 19 failures is a *final* failure after the app's own
retry (`2a tentativa`, 6–10 s each), across ≥3 distinct hosts, spanning far more
than 3 s with no success in between (first success at ~20 s). That satisfies the
health rule (3 failures, 2 hosts, 3 s), so even with the retry fix the state goes
offline. The reland only removes false positives from *intermediate* attempts.
(The message itself cannot be confirmed from the log: no `saude=` line exists in
this build.)

### Options considered

- `CURLOPT_IPRESOLVE=V4`: not chosen. Every failure is "Resolving timed out"
  from the resolver (many unrelated hosts fail identically, and later
  requests succeed with the same options); no evidence of a hanging AAAA, no
  c-ares version in the log to support it. Cheap to try later if the user reports
  IPv6/custom DNS.
- Shared DNS cache (`CURLSH`): not chosen. The failure is the *first* lookup per
  name; a cache does nothing for it, and the per-thread handle design (reuse
  tests) would need locking. Not justified by this log.
- Chosen (a): health ignores pure DNS timeouts (curl 28 with `Resolving timed
  out`) during the first 60 s after the first network note, until any request
  has succeeded. After 60 s without a success, or once something succeeded, DNS
  timeouts count normally, so real outages still show "No internet".
- Chosen (b): the log upload (`rede_postar_repetindo`, 3 attempts, 1 s/2 s
  waits) retries only when no HTTP status arrived and the attempt did not
  consume its whole budget (>75 % of the 30 s limit stops it), so a user is not
  left waiting more than once.

### Validation

`tests/rede_saude.sh`: new `testarDnsLento` — three DNS timeouts from distinct
hosts at boot leave health online (FAIL before: offline, assertion); the upload
retries twice and succeeds on the 3rd attempt (FAIL before: no retry helper);
after 60 s with no success DNS timeouts still declare offline; an HTTP 503
reply is not retried. Also `redesaude.sh`, `rede_reuso.sh`, `rede_parada.sh`,
`tools/mac.sh`. Not verified on a TV.
