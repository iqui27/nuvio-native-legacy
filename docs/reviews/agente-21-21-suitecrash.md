# 21-21-suitecrash: CRASH list from 21-suite-novas

| Test | Class | Cause | Fix | Commit |
|---|---|---|---|---|
| desktop-r9 | CRASH (test harness) | `desktop_native_events.m` lacked stubs for app calls added by the native sidebar (fbb448224, 6fb8eee66) | inert stubs in harness; assertions unchanged | 2c153af61 |
| guialayout | noise | passes on rerun (disk-full run) | none | - |
| guiav5_bugs | CRASH (test harness) | draw helpers added to guiav5ui.c by f254bbf54/fc92c0b91 (`t21_ilha_folha`, `plrui_linha_foco`, `plrui_kicker`, `ajustes_ui_interruptor`) were not macro-stubbed like the other draw calls, so the real ones ran with no GL context | no-op macros like the existing ones; assertions unchanged | 25fc1f9cb |
| guiapolir_related | aggregate | its crash was the guiav5_bugs step (fixed above). Now fails later in `guia_previa_grande` -> `tests/guiapreview_android.py`: Kotlin slice of NvPlayer.kt misses `relatarPreviewAudio`, `larguraLogica`, `alturaLogica` (compile error, not a crash) | not fixed here: guia_previa_grande is in the ASSERT set | - |
| usbsonda | noise | passes on rerun | none | - |
| ajustes_ux_toque_modo, _desktop | CRASH | NOT fixed in base: both still segfault (rc 139) | out of scope (separate branch) | - |

No app code changed. `tools/build-local.sh` rc 0.
