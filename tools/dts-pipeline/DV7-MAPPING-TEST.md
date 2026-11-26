# P7 mapping option (v24, no diagnostic QR)

Base: `build/playback-v17-no-qr`, commit `65499610db565ca038b0d0ca130585f238d59911`.
Ported from `build/dv7-mapping-test` into `build/playback-v17-no-qr`.

In Settings > Playback > sound/video, enable P7 to P8.1 conversion, then use
**Dolby Vision P7 mapping (experimental)** immediately below it. Default OFF is the
existing Mode 2 conversion. ON uses libdovi's named `To81MappingPreserved`
variant (library enum index 4, dovi_tool CLI Mode 5; numeric `4` must not be
passed to `convert_with_mode`, because its numeric adapter selects P8.4).
Reopen the source after each setting change. Compatible MKV and MP4 P7 use
this snapshot; native P5/P8, HTTP buffer sizes, RAM safety, seek and audio
reuse remain unchanged. Enhancement-layer pictures, including FEL, are still
discarded. This is a diagnostic comparison, not a promised image upgrade.

Use the same P7 FEL and MEL samples, fixed scenes, picture settings and buffer
preset for OFF/ON comparisons. Check colors, brightness, gradients and black
levels, then seek and switch DTS/TrueHD/AC3/EAC3 tracks. Test native P8 and SDR
as controls. A Dolby Vision logo only confirms mode activation, not accuracy.

The red-button log panel retains mapping messages. This branch has no diagnostic QR export; ordinary phone text input is retained.
After source open, `[dv7] RPU mapping=...` identifies the session snapshot.
The first successfully converted RPU also logs `P7-to-P8`,
`mapping-retained`, `residual-disabled`, and `differs-from-standard`.
The last field compares actual converted RPU bytes with a standard conversion
of that same input; MEL/identity mappings may have no byte difference.
The witness is computed once per open, never per picture, seek or audio switch.
It does not decode video, create another reader or expand the existing buffers.

Tests cover Rust RPU curve/DM preservation and residual removal; byte-identical
OFF output; bounded C packet processing; real MKV and dual-track MP4 output,
seeks/audio changes, native P8 unchanged; settings defaults/local scope/build
gates; session snapshot isolation; diagnostic QR absence and normal phone input.
