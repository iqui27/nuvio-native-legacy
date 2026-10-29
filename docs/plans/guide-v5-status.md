# Guide v5 implementation status

This branch is a partial implementation, not a completed delivery of the approved v5 mockup. Do not merge it as full v5 acceptance.

## Implemented and checked

- Held Up/Down scroll channels across groups without opening the category drawer; SDL repeats and webOS repeat=0 are covered by the real event-handler test.
- Native source wizard routes for type, credentials, M3U URL/file handoff, connection test, content selection, categories, XMLTV, import confirmation and completion. Draft Xtream credentials are not saved before confirmation.
- Bounded Xtream JSON catalog parsing, movie/series selection and episode parsing. Catalog memory is capped at 768 items, currently 256 per kind. Truncation is disclosed; full pagination is not implemented.
- Streamed M3U HTTP parsing, bounded lines/body, local phone file import up to 3,000 bytes, and persistent local source choices. Larger files require a URL.
- Phone setup uses the existing expiring, single-use LAN token server, exact Host/Origin checks and no-store responses. The HTTP test checks a real POST and restoration of the generic keyboard page.
- Profile PIN verification uses perfis_verificar_pin on a worker. Scoped leases require successful verification, expire, and are revoked on profile/account changes. Guide playback, previews and search check restrictions. Native profile-infantil flag is reused.
- Two, 1+2 and 2x2 multiview layouts retain one player session. Other tiles use static source artwork, never additional decoders. The decoder/provider cap logic is tested. Actual device/player playback remains unverified.
- New copy is in all language tables. Existing out-of-order master/sister rows were sorted together. Portuguese screenshots explicitly select pt-BR. Translation quality has not received human review.
- Account logout deletes guide v5 source/rule files. Guide publication rejects stale profile/account results.

## Acceptance gaps

- Native layout is visibly simpler than the approved mock. Top navigation, setup row details, fields/keyboard composition, source table, catalog hero/shelves, seasons/episodes, PIN keypad, kids cards and multiview controls are not visually equivalent.
- Source manager currently exposes the existing singleton Xtream source plus one local M3U source, not an arbitrary list of independently named providers.
- Phone connection testing, provider category discovery, per-step TV progress and final successful import happen on the TV after submission. The phone completion page truthfully says data was sent, not that import succeeded. First phone form is 883 px tall, rather than 844 px.
- Catch-up import/playback, trusted IMDb/TMDB title integration and provider resume shelves are not implemented. No title-name matching is attempted.
- VOD search covers the loaded provider inventory. It is not yet a unified channel/EPG/catch-up/account search. Native detail integration and full provider metadata remain incomplete.
- Parental editor supports one blocked group and one blocked channel, not multiple rule lists. Rating/age data is treated as unknown unless trustworthy; a review score is never used as an age rating. Broad profile unlock/channel-only lease UI is incomplete. Security integration needs independent review before release.
- XMLTV uses the existing single extra-guide slot; provider-first merging with an additional XMLTV source is not implemented.
- M3U headers, richer relative URLs, full category inventory and stable provider channel numbers need further work. File upload ceiling is small by design of the current pairing transport.
- No verified multi-decoder platform path, stream snapshots or per-device measured decoder detection. One video is the honest supported limit. Provider connection counts may include the current session; cap handling is conservative.
- Shot tests use offline fixture metadata; artwork readiness varies. They prove nonblank rendering and generate comparisons, not visual equivalence. Three phone captures cover the served form states; phone import success is not claimed.
- No target TV FPS/memory measurements, real provider/account integration tests, push, deployment or installation.

## Evidence

- Target suite: `bash tests/guiav5.sh` (parser/rule/decoder assertions, episode fixtures, real LAN HTTP pairing and held-arrow handler).
- Build: `bash tools/build-local.sh`.
- Localization: `python3 tools/varredura-i18n.py` and `python3 tools/idiomas.py`.
- Screens: `bash tests/guiav5_shot.sh`, `python3 tests/guiav5_compare.py`.
- Outputs: `/Volumes/ExternalSSD/tmp/rev/guiav5-png/`, each `*-compare.png` is approved mock on the left and actual native app on the right. All comparison groups were opened. They are nonblank and show the gaps above.
- The mock PNGs are English; native app side is pt-BR as requested. Original approved mock files were preserved.
