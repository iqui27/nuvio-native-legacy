# Personal server integration status

Implemented on `agente/21-biblio-mock-servidores-impl` against the owner-approved `/Volumes/ExternalSSD/tmp/servidores-polish` mockups. This is a partial implementation, not a visual acceptance claim.

Implemented: Library origins and provider filtering; cached membership badges; matched-title dedup; private unmatched items confined to provider filters; native catalog pagination bounded by CAT_MAX; Plex PIN/account resources/manual identity verification; Jellyfin Quick Connect; Jellyfin/Emby public users and password login; UDP LAN discovery; Emby Connect PIN/password/server exchange; generation/profile checks; library selection in provider credential files; selected-library index scope; list collections/playlists and drill-down; Home row addition/removal; Detail chips opening Sources; provider marks in Sources; pt-BR UI with six new entries translated in every table.

Validation fixtures exercise Library membership, selected-library ID matching, collection/playlist key construction, invalid discovery URLs, Connect credential variants and HTTP request shapes. Hidden GL shots use synthetic, account-free data and the production renderers. External comparison images place the approved English mock on the left and the native pt-BR rendering on the right; references have not been relabeled as translated or matching.

## Remaining acceptance gaps

- Pixel parity is not achieved. Setup copy, shell/clock/rail, user layout, fields, keyboard placement, state descriptions and row editor differ from the approved mock. Shared keyboard/pairing opens as the existing modal.
- Home row setup does not yet expose the approved content/style/order/hide-watched controls. Explicit collection/playlist rows now use the server pager; live end-to-end proof remains missing.
- Offline catalog metadata is not persisted for the new Library flow; last-sync timestamps and dedicated offline/expired recovery screens are incomplete.
- Detail provider chips open the ordinary Sources picker; provider-specific source filtering and exact source-sheet composition remain incomplete.
- Watched synchronization is existing playback reporting, not a new two-way import of all watched history. No synchronization toggle is presented.
- Emby Connect AccessKey is persisted in the existing protected profile credential file and refreshed on library reload. Live vendor login/reconnect was not tested.
- Existing adapters cap available libraries at six per provider. Catalog loading is bounded by CAT_MAX and startup/selection memory has not been measured on weak TVs.
- No TCL Mali-G52 or webOS 3 frame-budget measurement, device validation or independent review.
- PNG evidence covers setup and Library states; dedicated final Detail/Sources/Home-row captures and every requested mock state remain incomplete. Some shots depict sparse fixtures, not full API catalogs.

No push, deployment, stash or real-account mutation was performed.
