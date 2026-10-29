# AIOStreams implementation and remaining acceptance work

Owner reference: `/Volumes/ExternalSSD/tmp/aiostreams-mock/index.html` and its 15 PNG routes. The owner approved replacing the former layouts. The current implementation is **partial and must not be accepted as full mock parity**.

Implemented:

- Native setup: public host selection, own server origin, official configure QR, existing LAN phone pairing for a masked manifest import, validation and local installation.
- Manifest/resource/catalog inventory, authenticated read-only User API summaries when the standard personalized URL supplies credentials, explicit manifest-only fallback for aliases/variants or unavailable APIs.
- Dashboard sections for catalogs, sources/services, links, subtitles, filters, health and title inspection. Addons entry opens management; Sources chip opens title inspection.
- Home visibility/order uses existing local row controls. Local P2P policy uses existing settings. Advanced editing remains on the official configure page.
- Network/JSON work happens on a worker. UI drains an account/profile/generation-scoped snapshot. Requests do not probe playback URLs. Retry-After delays refresh. No manifest URL, Basic header, config credential or media target is rendered or logged by this module.
- Imported addons use the existing account/profile addon snapshot for restart; the normal account pull can replace this local list. Signed-out installations are memory-only.
- External-only results are excluded from the generic playable stream parser and counted in inspection.

Evidence:

- `bash tests/aiostreams.sh`: offline parser and real UI transition tests, including URL redaction, credential headers, manifest rejection, actual upstream ID, secret-free summaries, external/P2P inventory, masked import, keyboard result consumption and profile isolation. Initial missing feature and external-link regression both failed before their fixes.
- `bash tests/addons_sync_ui.sh`: existing addon list/sync regression adapted to the new AIOStreams entry.
- `bash tests/stream_parser.sh`: generic parser regression.
- `python3 tools/idiomas.py`: table alignment, byte order, formats and glyph checks.
- `python3 tools/varredura-i18n.py`: all three categories zero.
- `bash tools/build-local.sh`: native host build.
- `bash tests/aiostreams_shot.sh`: 15 native pt-BR fixtures, no real account. This compiles the complete app without main.
- `node tests/aiostreams_compare.mjs`: checks dimensions/nonblank native captures and produces mock-left/app-right PNGs in `/Volumes/ExternalSSD/tmp/rev/aiostreams-png`.

Remaining acceptance work:

1. Match the approved mock precisely. Setup uses the same split-island structure, but text scale, surface treatment, controls and section details differ. Sources integration still uses the existing narrow sheet geometry. The owner explicitly requested its replacement; adding a chip alone does not meet that requirement.
2. Complete read-only configuration detail: full required/included/excluded precedence, size/bitrate ranges, seeders, templates, formatter/proxy semantics and known schema variations. Current summaries expose selected values and field counts, not a complete rule viewer. Never present excluded resolution values as required values.
3. Complete subtitle inventory and per-title subtitle results/flags. Only explicitly declared preset subtitle restrictions are attributed; the manifest alone cannot identify all wrappers. Embedded tracks remain in the player.
4. Complete external-link handoff. Links currently shows types/counts and the official configure QR; it does not transfer an individual external/NZB/YouTube target to the phone.
5. Connect title inspection to actual Nuvio keep/hide/rank evidence. Current view preserves returned positions and explains known cache/external/P2P states; it does not calculate or claim the mock's full Nuvio rank trace or upstream rejected-candidate list. No Search API statistics/errors integration yet.
6. Complete status capability/version presentation, subtitle request timing and rate-limit headers. Current health shows locally measured status/timing of manifest/status/config/stream requests, not all upstream metrics.
7. New phrases are pt-BR plus English. All 28 additional tables contain aligned English fallback entries; native translations remain outstanding.
8. Test real server compatibility, phone handoff and weak-TV frame budgets (TCL Mali-G52/webOS 3), both solid and glass materials. Host PNGs and static work limits do not prove TV performance. No deployment or push performed.

The upstream docs/source contract was checked at `89312b535f7e1eda9c81b1f0de3b62730bd7e22e`. Default manifest ID is `com.aiostreams.viren070`; branding and user-visible names can vary. Authenticated User API is a GET only; no create/update/delete configuration request exists in this implementation.
