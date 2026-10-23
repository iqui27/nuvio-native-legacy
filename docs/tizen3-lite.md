# Experimental Tizen 3 lite

## Verdict

**GO for further isolated experiments; NO-GO for shipping or claiming 2017 TV support.**
The complete application builds as JavaScript and renders the unconfigured login
screen without native WebAssembly. This establishes a viable compilation path,
not acceptable TV performance, authenticated operation, or working playback.
No TV was connected or installed. No certificate, signing tool, deployment, or
push was used. Normal WGT defaults, its manifest/packager, and TPK builds remain
unchanged; all new C behavior is behind opt-in compile definitions.

## Runtime feasibility and prior art

Samsung's [web engine table](https://developer.samsung.com/smarttv/develop/specifications/web-engine-specifications.html)
identifies 2017/Tizen 3.0 as Chromium M47. It has WebGL 1, typed arrays, Promise,
fetch and IndexedDB, with partial ES2015 support. It is not ES5-only, but cannot
parse ES2017 async functions or later syntax. Native WebAssembly and
SharedArrayBuffer/pthreads are unavailable. ES5 is the conservative emitted
syntax target; this does not supply missing browser APIs.

No preexisting asm.js target was found. `tools/tizen.sh --vidaa --um-fio`
already uses `src/fio1.c` cooperative fibers and Asyncify without pthreads, but
still emits WASM. The existing Tizen renderer already caps WebGL at 1. The webOS
legacy/native paths do not remove the WGT browser's dependency on WASM.
`emcc` generates glue from the C sources and EM_JS/EM_ASYNC_JS bodies; normal
WGT glue is lowered by esbuild to Chrome 69. That alone is insufficient here.

Emscripten 6.0.9 accepts `-sWASM=0 -sLEGACY_VM_SUPPORT=1`. This uses **wasm2js**,
not fastcomp's historical validating asm.js backend. The emitted script contains
a local `WebAssembly` JavaScript shim; its name and calls are expected. There
is no `.wasm` file and no dependency on the browser's native implementation.
Babel lowers only the surrounding glue, preserving the enormous wasm2js body
byte for byte. Parsing the entire body with Babel exceeded Node's 4 GiB heap
in the interrupted run. `-O2` also exceeded a ten-minute local build limit;
the experimental target uses `-O1`.

[AVPlay](https://developer.samsung.com/smarttv/develop/api-references/samsung-product-api-references/avplay-api.html)
is documented since Tizen 2.3 and exists on Tizen 3. This is an API baseline,
not a guaranteed firmware player-library version. Query `webapis.avplay.getVersion()`
on the eventual test TV; no value was measured here. Newer optional methods
and firmware-specific behavior in the shared Samsung backend remain unverified.
Samsung documents [HLS, DASH and Smooth Streaming](https://developer.samsung.com/smarttv/develop/guides/multimedia/adaptive-streaming.html)
through AVPlay. Its [general specifications](https://developer.samsung.com/smarttv/develop/specifications/general-specifications.html)
show HLS MPEG-TS and fMP4 (fMP4 since 3.0), with per-tag limitations: EXT-X-MAP
since 3.0, low-latency HLS only on newer platforms. DASH fMP4 support does not
imply every MPD, codec or DRM scheme works. Use the
[2017 codec/model table](https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2017-tv-video-specifications.html)
for the actual TV. No HLS/DASH stream or DRM session was tested.

No portable, guaranteed web-app RAM allowance was established. The fixed
128 MiB C heap excludes JavaScript code/JIT, decoded images, browser and GPU
allocations. Desktop telemetry reported roughly 326 MiB JS heap and a 128 MiB
linear buffer; these figures must not be added as disjoint allocations or
used as a TV memory guarantee. A 94 MiB script is a major warning for M47.

## Build and output

From the worktree root:

```sh
bash tools/tizen3-lite.sh
bash tests/tizen3-lite.sh
```

Both final commands returned **0**. Build log: `build/tizen3-lite-build.log`.
The wrapper explicitly enables diagnostic/unconfigured compilation because
this checkout has no account-server configuration. Configure the existing
`NUVIO_PROPERTIES` mechanism for a future authenticated experiment; the local
artifact tested here cannot sign in. The wrapper never signs packages.

Tools used: Emscripten 6.0.9, Node 22.22.1, Python 3, zip, Babel core 8.0.7,
Playwright 1.58.2 and its Chromium 145.0.7632.6. Babel core and the transform
plugins listed in `tools/tizen3-es5.py` must already be installed under
`$HOME/node_modules`, or set `NUVIO_BABEL_NODE_PATH` to their node_modules path.
The focused gate also uses the Acorn shipped with the emsdk and a host C compiler.
No new project dependency or lockfile was added for this spike.

| Artifact | Bytes | MiB |
| --- | ---: | ---: |
| `build/tizen3-lite/NuvioTV003.wgt` | 44,358,238 | 42.30 |
| `build/tizen3-lite/index.js` | 99,049,101 | 94.46 |
| `build/tizen3-lite/index.data` | 32,090,665 | 30.60 |

Manifest: `required_version="3.0"`, package `NuvioTV003`, application
`NuvioTV003.NuvioLite`, widget `http://nuvio.tv/tizen3lite`. These differ from
normal package `NuvioTV002`. The unsigned archive contains only index.html,
index.js, index.data, config.xml and icon.png. It has no signing files.
A checksum stamp rejects missing/modified artifacts on `--package-only`.

## Cuts and retained behavior

- Glass/frost, dynamic backgrounds and 4K UI disabled through the existing
  safe-setting accessors; normal static artwork remains.
- All trailer playback disabled, including manual and automatic trailers.
- P2P remains disabled by the existing web-target policy; no native engine.
- No pthread pool, SharedArrayBuffer, OffscreenCanvas or decode worker.
  Cooperative fibers reuse the VIDAA path. Plain-array Atomics helpers serve
  single-thread JS call sites; they are not a shared-memory implementation.
- Image decode falls back to an Image/Blob URL and main-thread canvas.
- Fixed 128 MiB linear heap, 4 MiB main stack, 128 KiB Asyncify stack;
  32 MiB texture budget remains capped after account/local settings apply.
  Cards default to 320 px, all image requests cap at 640 px, hero at 640 px
  across quality settings. Existing image sources/fonts are retained, so
  this is not a minimal asset package.
- `NV_LEVE` cuts notice-channel work, recommendations, periodic sync and focus
  GIF jobs. libass is disabled; rich ASS rendering is not part of this spike.
- Samsung AVPlay backend and transparent video-plane shell are retained,
  with a separate minimal shell and remote-key registration.

## What the smoke proves

The focused gate parses all generated JS and inline scripts as ES5, checks asm
preservation (including a brace inside a string), compiles a production hero-cap
probe, validates the unsigned archive identity, and starts the full application.
It blocks outbound requests and uses an ephemeral loopback server inside the
foreground test process, closed on completion. No persistent server is launched.

Chromium is launched with `--js-flags=--noexpose_wasm`. **Chromium 145 still
exposes native WebAssembly with that flag**, verified separately. The harness
therefore deletes `window.WebAssembly` before any application script and asserts
its absence. It also removes globalThis, createImageBitmap, OffscreenCanvas,
SharedArrayBuffer and Atomics; the shipped shell/glue must supply needed fallbacks.
The final run rendered 226 WebGL frames and continued advancing after five
seconds, with no page exception. IDBFS mounted, fonts/art loaded, and both
texture-budget log entries stayed at 32 MiB. Desktop has no AVPlay substitute.

Screenshot: [no-wasm.png](shots/wgtlite3/no-wasm.png).
Machine-readable evidence: [smoke.json](shots/wgtlite3/smoke.json).
This is a modern desktop engine with selected APIs removed, not M47 emulation.
The screenshot honestly shows the missing-server login state.

## Blockers and next evidence

1. Run on actual M47 firmware: parsing/JIT time, GPU shader behavior, texture
   pressure, sustained navigation and watchdog/OOM behavior remain unknown.
2. Configure an account server and prove login, catalog, addons, image-heavy
   scrolling and persistence. Smoke blocks external requests, so its expected
   network failures are not service-integration results.
3. Measure AVPlay version and test HLS/DASH/MP4, codec/DRM selection, seeks,
   subtitles and Return/media keys on a specific 2017 model/firmware.
4. Audit remaining modern browser APIs beyond startup: old TLS roots, streaming
   fetch fallbacks, JS plugins and long-lived account/sync flows are unproven.
5. Reduce code/asset size or justify the memory cost on hardware before shipping.
   ES5 parsing and desktop FPS cannot establish acceptable TV performance.

## Manual sideload procedure (not executed)

The unsigned output cannot normally be installed directly. A future operator
must use a **separate disposable Samsung TV developer certificate/profile**,
including the test TV's DUID, and sign a copy of `build/tizen3-lite-stage`.
Never use release keys or overwrite the normal package/staging directories.

On the TV, open Apps and enter 12345 to enable Developer Mode, supply the
operator computer's LAN address and reboot. With Samsung TV extensions and
the disposable certificate configured in Tizen Studio, connect Device Manager
to the TV on port 26101 and permit installation. An operator may then use:

```sh
sdb connect TV_IP:26101
sdb devices
# Replace the target/profile placeholders with values from the operator's tools.
tizen package -t wgt -s DISPOSABLE_TIZEN3_PROFILE -- build/tizen3-lite-stage
tizen install -n NuvioTV003.wgt -t DEVICE_NAME -- build/tizen3-lite-stage
tizen run -p NuvioTV003.NuvioLite -t DEVICE_NAME
```

Tizen CLI may choose an archive basename from widget metadata; use the actual
signed filename reported by `tizen package` if it differs. Connect only a
sacrificial test TV with explicit authorization. None of these commands were
run during this job. The distinct package ID permits coexistence with normal
Nuvio; uninstall only `NuvioTV003.NuvioLite` after the future test.
