# Menu visibility and performance

Validated locally with `bash tests/menu_perf.sh`, `bash tests/ajustes_catalogo21.sh`, and `bash tools/build-local.sh`.

Sports is an append-only, device-local switch (`menuEsportesLocal`, default enabled) in the existing sidebar settings section. Shared visibility filtering removes it from sidebar and top-menu rendering, focus order, pointer targets, and geometry. The current native iOS tab list contains Home, Search, Guide, Library, and Profile; it has no Sports entry. The Live TV Sports tab is unchanged.

Focus movement only changes the highlighted row. Screen navigation already waits for confirmation; there are no screen preloads on highlight movement. Home catalog reconciliation and artwork-reference updates now wait until 150 ms after menu input. Home treats the open menu as an overlay, preventing hidden hero rotation while retaining animation updates. Existing text and icon caches remain responsible for texture ownership. Menu labels now use one white texture with shader tint rather than separate focused/unfocused textures. Top-menu geometry is cached across focus animation, invalidated by visibility, viewport, scale, language, and font changes. Update and drawing measure at the same menu scale.

The supplied TCL log contains a 279.0 ms worst frame and a separate 68.3 ms draw, both in Settings. Its worst-frame text counters and upload counters are zero. Those samples do not establish that highlight handlers caused the stalls. No device was deployed or profiled after the change.

The offscreen 1920×1080 Mac harness runs the original menu from commit `00fd00ba5a37c116eaeacb8d6e81791829186c9c` and the current menu against identical dependencies. Each layout runs 360 frames, 45 focus moves, and three open/close cycles, including GPU completion and first focus on each row. Springs, fades, scrolling, and glass stay enabled. This isolates menu cost; it does not measure the complete application or predict TCL frame times.

| Layout | Before p50 / p95 / max (ms) | After p50 / p95 / max (ms) | New text textures before / after |
| --- | --- | --- | --- |
| Sidebar | 0.682 / 1.177 / 12.110 | 0.725 / 0.940 / 5.539 | 19 / 11 |
| TV | 0.383 / 0.816 / 3.251 | 0.387 / 0.660 / 3.367 | 20 / 12 |
| Top | 0.424 / 0.733 / 1.714 | 0.331 / 0.713 / 1.584 | 0 / 0 |

The harness also checks that focus moves never navigate, the input deferral expires, and persisted Sports visibility affects TV/top order. It runs the existing hidden-menu navigation regression. The catalog test verifies Sports is reachable, switches through the existing Settings interaction, and survives disk reload. Catalog result: 417 options routed, six sub-editors, eleven documented removals.

Pending: complete application measurements on TCL, native iOS runtime validation, and broad repository test suite. Local menu timings alone do not prove the reported 260 ms device stalls are eliminated.
