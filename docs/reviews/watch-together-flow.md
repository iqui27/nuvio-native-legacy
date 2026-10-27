# Watch Together lobby flow

Entry from Detail and episode actions now opens the approved lobby immediately. The exact title/episode is queued through the existing source resolver; playback prepares paused behind the lobby. The room is created after the first frame and resume position are ready. Normal Play and guest Start cannot bypass the host's lobby action. Next episodes also wait for explicit lobby Start.

The existing room protocol now broadcasts measured RTT, clock offset, drift and public quality once per second, alongside readiness and buffer percentage. Unknown measurements remain unknown. Seats animate by identity and pulse on readiness. The host can start while guests load; they catch up through the existing sync engine. Start schedules three seconds on the server clock and renders 3–2–1 over the lobby. Playback retains an avatar/drift chip. Existing soft correction, offline presence, late join and host succession remain in the sync layer.

No dependencies added. Source URLs stay private. Existing translations reused; i18n scan reports zero missing strings. The C regression includes the actual app adapter plus three production sync motors; the JS regression uses the production room logic. The simulator and room suite cover drift correction and lifecycle behavior.

Evidence directory: `/Volumes/ExternalSSD/tmp/rev/21-21-juntosfluxo/`. Native pt-BR PNGs include preload, arrival frames/strip, loading, ready, offline, invite QR, countdown and player chip. PNGs were opened for inspection. The host renderer uses fixture artwork and simulated readiness, not a physical decoder.

Validation: `bash tests/juntos_fluxo.sh`; `bash tools/build-local.sh`; `python3 tools/varredura-i18n.py`. The initial regression failed on the old immediate-play entry, then passed with the lobby flow. Build passed with existing compiler warnings.

Limits: physical TV buffering and start latency were not measured. The updated room service must be released with the client for public telemetry and the full three-second countdown; nothing was pushed or deployed. Samsung C# changes were inspected but not compiled by the Mac C build.
