# AIOStreams dashboard: manifest configuration and TV overview

The dashboard worker previously fetched configuration only when it could derive Basic authorization from a two-segment `/stremio/` installation. It never decoded embedded manifest configuration, including installations stored through the shared long-URL alias table. Its parser accepted only `data.userData`. Existing fixtures exercised that authenticated envelope and missed the manifest-only worker path.

The worker now expands aliases and reads base64url JSON embedded immediately before `manifest.json`, including percent-encoded alphabet characters. Only known configuration fields enter the existing sanitized snapshot. Encrypted tokens are not treated as configuration. API authorization remains separate from embedded configuration. A masked UUID/password action supplies read-only API access for the current screen; credentials are not persisted and are erased on close/profile exit.

Opening the dashboard has no title ID, so it does not issue a stream/search request. It now reuses the most recent movie/series stream request from installation/profile-scoped history. Without history, health explicitly says the request has not been measured. Inspect title remains the action that performs a title request. Configured debrid services do not imply an active subscription, quota, or successful playback.

Requests log only `[aio]` endpoint kind, identity kind, a literal redacted URL marker, HTTP status, body bytes, transport error, and top-level shape. Section logs contain counts only. No response body, personal address, UUID, password, or debrid key is logged. Dashboard requests bypass caches and retain TLS verification.

The overview uses the existing 2.1 island materials, a status/version/update header, four counts, and six cards: add-ons, services, filters/limits, sorting/formatter, catalogs, health. Each card scrolls its own list with a spring, count, and scrollbar. Direction keys move between adjacent columns/rows after reaching a list boundary. Existing entry, focus, page fade, and exit animation remain in use. Empty configuration sections explain the connection action; offline cards offer refresh.

## Validation and evidence

`bash tests/aiostreams.sh` first failed at the manifest-only worker assertion, then passed after the fix. The sanitized fixture also covers long-URL alias expansion, hidden service keys, card navigation, masked credential entry, and rejection of malformed/encrypted payloads. Full-config and error captures use the existing native screenshot harness at 1920 x 1080.

Evidence directory: `/Volumes/ExternalSSD/tmp/rev/aiodash2/`. Before and after PNGs exist for full-config, manifest-only, and error states; `montage.jpg` compares before (left) and after (right), 2232 x 1905. Before manifest-only capture represents the old parser's absent config; after capture uses the decoded sanitized fixture.

TCL installation and LEANBACK launch succeeded for the diagnostic build. No TV navigation keys, uninstall, or data clear were used. Log reads contained no dashboard `[aio]` entries. Therefore the owner's exact live identity, HTTP statuses, response shape, TLS behavior, and actual missing-data cause remain unconfirmed. The regression uses the known API/configuration shape and synthetic sanitized data, not a captured account response.

Remaining limits: instance aliases/variants do not yield guessed Basic credentials; opaque/encrypted configuration cannot be decoded locally. Non-English locale tables use existing fallback for new strings. Long individual remote labels may still ellipsize; full configuration detail views remain accessible. Device navigation and animation playback await owner interaction.
