# i565 — PenguPlay (pengu.uk) "addon not working" on Tizen .wgt 2.0.6

## Report
https://pengu.uk/ addon works elsewhere, fails in the app on Samsung Tizen 2.0.6.

## Root cause
Same class as #552. In 2.0.6, `fonteOferecida` (src/streams.c) hid every
stream with `behaviorHints.notWebReady` when P2P was off (introduced in
b658b41b3). On .wgt, P2P is hidden/always off, so all of PenguPlay's catalog
vanished — every authenticated stream it serves is `url: https://pengu.uk/...`
+ `notWebReady: true` (MKV/MP4 the browser cannot play but native players can).

Fixed on this branch by d25a3e513 (`pedeP2P`/`fonteOferecida` in src/streams.c):
notWebReady with a public http(s) URL no longer requires P2P.

## Verification (tests/pengu.sh, no network)
Fixtures: `tests/fixtures/pengu/manifest-auth.json` (id `com.penguplay`,
object resources, `tv` type, `tmdb:`/`tt`/… idPrefixes, config array) and
three real authenticated bodies (token redacted before capture; not in repo):
movie `tt0111161` (28 entries), movie `tmdb:278` (28), series
`tt0903747:1:1` (27).

- Manifests install: base URL handling for root/token/query variants; the
  manifest-auth (config + `behaviorHints:{}`) probes identically.
- `stream_extrair` returns 27/27/26 playable streams — the leading
  `{externalUrl: "https://pengu.uk/donate"}` card (no url/infoHash) is skipped,
  matching Stremio behavior.
- Every parsed stream: `https://` url, `notWebReady`, `fonteOferecida` true
  with `p2p_ativo() == 0` — also under `-D__EMSCRIPTEN__` (WGT config,
  stub emscripten.h).
- `filename` containing `|^|` and 28 GB `videoSize` parse fine
  (`arquivo`, `tamanhoBytes`).
- FAIL side: the v2.0.6 `fonteOferecida` (extracted via `git show
  v2.0.6:src/streams.c`) hides 27/27 streams of the real capture → exit 1
  (expected FAIL). Current code → PASS. Fail-to-pass demonstrated.

No source change needed; regression test only.
