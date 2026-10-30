# Desktop verification

Artifacts are local in `/Volumes/ExternalSSD/tmp/rev/desktop`:

- `Nuvio.app`: arm64, macOS 27.0 or newer, ad-hoc signed. Homebrew libmpv plus
  50 non-system dylibs are copied, relinked and signed. `mac-mpv-deps.json`
  records the architecture intersection and deployment minimum.
- `Nuvio-mac-arm64.dmg`: mounted read-only and verified. The previous universal
  package contained the simulation backend; it was moved into the output cache
  as `superseded-stub-Nuvio-mac-universal.dmg`.
- `Nuvio-win64.zip`: x64 executable, upstream `libmpv-2.dll`, requested
  `mpv-2.dll` alias, Vulkan loader and SDL/curl dependencies. Every transitive
  DLL import is checked against binaries extracted from the ZIP.
- `desktop-artifacts.json`: archive sizes, SHA-256 checksums and source revision.

The official Windows mpv development archive and Vulkan runtime are pinned and
checksum verified before extraction. The Mac release requires compatible
Homebrew libraries to support an older OS or Intel; this build does not claim
those platforms.

## Reproduction

Run from the worktree root:

```sh
bash tests/desktop-final.sh
bash tests/desktop-build.sh
python3 tools/desktop-capture.py
python3 tools/desktop-wine.py
python3 tools/desktop-proof.py
python3 tools/desktop-proof.py --mount-only
```

`desktop-build.sh` builds local Mac UI, offline Android arm64/armv7 debug APK,
and both desktop packages. `desktop-packages.sh` rebuilds only desktop packages
after desktop fixes. Build properties remain external. No publishing or device
installation is performed.

`desktop-final.sh` checks responsive geometry and sidebar routes, repeated modal
selection, keyboard/gamepad translation, fullscreen and thread portability. Its
real SDL/libmpv smoke checks decoded pixel orientation, alpha composition,
source cropping, leaked scissor/color-mask isolation, audio switching, embedded
and external subtitle pixels, seeking, EOF/error and rapid replacement. It also
cross-links against the official Windows headers and import library.

The Mac capture launches the packaged app with an isolated offline catalog and
all existing tours marked as seen. It clicks title-bar Search, verifies the
Search screen through telemetry, clicks Home, resizes the window and plays the
Sintel trailer. Synthetic mouse events carry the real SDL window ID, allowing
correct conversion in an unfocused Retina window. No account data is copied.
The fixture remains outside the distributed app and ZIP.

The Windows capture launches the packaged executable through CrossOver/Wine
with isolated data, plays the same trailer and reads a framebuffer PNG.
The helper terminates its own Windows child as well as the launcher. Final
Mac and Windows screenshots show recognizable moving-video frames. The artifact
checker rejects black playback frames and verifies Search navigation.

## Evidence and limits

- `mock-vs-app.png`: approved 1280×800 mock viewport beside actual Mac app.
- `mac-home-1280.png`: wide sidebar, toolbar search, hero and catalog rows.
- `mac-home-narrow.png`: existing touch portrait hero and controls after resize.
- `mac-search.png`: toolbar Search opened through the pointer route.
- `mac-video.png`, `windows-video.png`: Sintel playback frames.

The desktop composition follows the approved structure and destination order.
Exact visual parity remains incomplete: existing TV metadata/card treatments,
icon choices, spacing details and selected hero artwork differ from the mock.
The narrow layout reuses the repository's existing portrait code. Physical
controller hardware and a native Windows host were not exercised; translation
and Windows playback under Wine were exercised. HDR passthrough was not tested.
The Android build passed; no TV device runtime test was performed.

Source review found and resolved playback reload speed correction, archive
closure verification, repeated modal destination selection and stale open-list
state. The final independent follow-up returned OK for those fixes.
