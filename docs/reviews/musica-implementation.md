# Native soundtrack album implementation

Approved reference: `/Volumes/ExternalSSD/tmp/musica-visual/`, round 2. The detail soundtrack section now opens `musicaui.c`, a full-screen native album page. The owner-approved layout replaces the former two-track/save card behavior. Saved albums continue using the existing profile-specific Lists > Music storage.

The page uses seven tracks per page, Lucide controls, existing typography/cache/GLES2 primitives, entrance/focus/equalizer motion, reduced-motion states, and a mini preview bar. Spatial RGBA averages are captured during the existing image decode (1 KiB per texture-cache slot); the page creates one 32x18 RGBA ambient texture (2,304 bytes), once. Cover and mini cover share the cache texture. No full-screen blur target or per-frame audio/image allocation is introduced. Changing progress text uses cached digit glyphs.

Previews use `video_tocar()` directly, without player navigation, scrobble, or resume writes. The page blocks trailer acquisition, refuses active/retained main playback, waits for trailer release, clears source headers, sets the MP4 source hint and webOS lean load mode to bypass VOD buffering. Playback readiness accepts native playing/time events without requiring video dimensions. It restores the prior trailer volume policy and releases on OK, Back, end, error, 30 seconds, page replacement/exit, background, or quit. Sequential/shuffled queues contain only tracks with HTTPS previews. Desktop and unsupported backends hide preview actions.

The QR uses the existing worker phone resolver rather than the mock's frozen direct Apple Music URL, as required by the approved README. Vendor labels therefore include Spotify, Apple Music and YouTube. Long title URLs use canonical IMDb resolution to fit the existing QR encoder. The clock uses actual time, and focus uses the configured accent; the host captures use the existing Arctic Blue setting, which differs from the mock's sample color.

Validation:

- `bash tests/musica.sh`: paging, preview parsing, shuffle permutations, startup timeout, 30-second bound, stop rules, spatial color sampling, saved-profile isolation, and actual UI ownership against a fake native backend.
- `python3 tools/varredura-i18n.py`: zero missing UI keys.
- `python3 tools/idiomas.py`: all 29 translations validated.
- `bash tests/musica_ui_shot.sh`: actual native GL renderer at 1920x1080 in pt-BR, four states and mock-left/app-right comparisons under `/Volumes/ExternalSSD/tmp/rev/musica-impl-png/`.
- `bash tools/build-local.sh`: local build.

The paging test failed before implementation; missing preview/color APIs also failed before implementation. The resulting targeted suite passes.

Visual comparisons were opened individually. Geometry, artwork, copy, seven-row list, focused track, now-playing bar and saved toast are present. Remaining visual differences include approximate spatial ambient color spread, font weight/metrics, native accent, and the intentional production QR destination/vendor caption. Exact pixel equivalence is not claimed.

Target-device verification remains required for AAC decoding, uMS/AVPlay/TPK/Media3 session release and volume timing, frame budget, and platform-owned decoder/network memory. No worker deploy, push, or real-device playback validation was performed.
