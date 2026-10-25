# Guide v5 implementation

Approved specification: /Volumes/ExternalSSD/tmp/rev/guiatv-estado/v5/ (31 new states). Native C/SDL/OpenGL; pt-BR keys and all language tables. No push, deploy or stash.

- [x] Restore 133cccd0b held-arrow behavior, adapted to this older guide. Real handler test FAIL (assertion) -> PASS. Commit independently.
- [ ] Add bounded provider catalog/setup logic with target fixture checks: malformed provider data, unsupported playback URLs, capacity, profile isolation and decoder limits.
- [ ] Add native v5 screen rendering and D-pad/pointer actions. Reuse Xtream credentials, network worker, profile PIN verifier, phone token transport and existing player. Replace source setup presentation; no illustrative success counts or fake controls.
- [ ] Apply parental decisions before metadata/previews/playback; revoke leases on profile changes. Keep one live decoder and artwork previews.
- [ ] Capture native screens against approved PNGs, open comparisons, report gaps explicitly. Run one final target suite, i18n scan and local build; commit working steps.

Acceptance: real actions and honest unavailable states; no secrets in logs; no network/large allocations inside drawing; bounded indexing; held arrows remain channel scrolling. Phone transport is LAN HTTP with existing token/Host/Origin protections, not an invented cloud service. Device decoder capacity remains one until a verified multi-instance player exists. Native screenshots cannot establish TV frame timing.
