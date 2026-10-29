# Issue #612: QE65QN91B black screen, Tizen 6.5, 2.0.6

No device log or installed release artifact is available. The reporter's root
cause is **not established**. All investigation and checks were offline.

## Package and startup path

Tizen 6.5/7 uses `NuvioTpk65`, not the API11 `NuvioTpk` package. Its manifest
requires API 6.5, profile `tv`, and runs `NuvioTpk65.dll`. The project targets
`tizen90`, Tizen.NET API9, and compiles shared `Program.cs` with `NV_API9`.
This TFM selects the matching GLWindow API; it is not a Tizen 9 OS minimum.
Sibling manifests require 4 (`NuvioTpk40`, separate TVGLApplication/Program40),
6 (`NuvioTpk60`, API8), and 8 (`NuvioTpk`, API11).

Release privileges are internet, appmanager.launch, and network.get; current
HEAD also has externalstorage from `31a62b273`, after 2.0.6. The optional
text canary adds recorder only when explicitly built. No manifest or csproj
changed between v2.0.5 and v2.0.6. `tools/tizen-config.xml` changed only its
version; it belongs to the separate WGT build, not native TPK startup.

`tools/tpk.sh` copies the common 6+ `libnuvio.so` into `lib/` for Tpk65,
checks its presence in the ZIP, and compares packaged bytes to the built SO.
`tools/tpk/Dockerfile` builds ARMv7 NEON softfp, not aarch64/hardfp, using
Debian armel/glibc 2.28. SDL, fonts, WebP, libass and optional FFmpeg are static;
GLESv2, dl, pthread, m and rt are system dependencies. EGL is resolved with
dlopen (`libEGL.so.1`, `.so`, `.so.1.4`), avoiding an EGL DT_NEEDED name that
some TVs lack. P2P loads separately via dlopen. There is no release binary in
this checkout to verify its actual architecture, NEEDED entries, or contents.

`Program.Main` constructs NUI, then OnCreate/Arranca selects a verified staged
SO or dlopens the bundled absolute path. It registers video and voice bridges,
configures native drawing, and starts the C app thread. API9 creates a
transparent GLWindow, requests GLES2 without depth/stencil/MSAA, registers the
render callbacks, selects Continuous mode and shows it. The framework creates
the EGL context; the C callback captures it and hands it between render and
app threads. Returning 0 waits without an app frame. Existing #558 watchdog
shows diagnostics after 15 seconds without a first frame; it requires a live
UI event loop and cannot recover a native abort or firmware hang.

## Release delta and ranked candidates

- `6bd1c06ff` added eager optional Samsung TextField/InputMethodContext and
  microphone probing before native initialization/window creation; retained
  by `e29de85cd`. A native IME hang/abort bypasses managed exception catches.
  Current HEAD already defers these APIs (`cc49341e6`, #558). No duplicate fix.
- `1c8f99d86` adds static FFmpeg to the 6+ SO; `d56db6430`, `b1574f7a0` and
  `fe6516d41` adjust build/playback. Larger load/mapping pressure is possible,
  but no loader error or memory measurement establishes it here. Playback
  recovery/prewarming changes do not directly execute on fresh media-free
  startup.
- `8a53efc0b` adds optional model export; current HEAD isolates its failures.
  Host dimensions remain 1920x1080. No API9 GL configuration change appears
  in the release delta.
- Wrong artifact, firmware GL failure, or library mapping denial remain
  possible. Source packaging rules do not establish which file was installed.

## Proven defects fixed and diagnostic coverage

The EGL loader previously returned success with only MakeCurrent and
GetCurrentContext present. Other unconditional calls could dereference NULL.
A partially populated dispatch also made the next attempt incorrectly return
success based on MakeCurrent alone. The regression executes production loader
code with each required symbol missing: before the patch it exits 1 with
`FAIL: loader accepted missing eglGetCurrentDisplay`; after the patch every
required symbol is validated, its exact name is retained, and retry reloads
symbols. Optional QuerySurface and SwapInterval remain optional.

A missing current EGL display/surface/context previously reached GL queries
and the app handoff. The callback now returns -1 before those operations and
persists the failure. Tests exercise all three missing states plus a valid
API9 startup that keeps waiting for the first app frame. These are proven
failure-handling defects, not proof that the QE65QN91B lacks an EGL symbol or
context.

`data/tpk-etapas.txt` now starts before NUI construction, using the standard
native app data-path getter (freed after copying). If unavailable, OnCreate
falls back to its existing DirectoryInfo path. Entry records process/OS
architecture; NUI construction/run/OnCreate, native library path/existence,
voice/apps registration, GL construction/config/show and context callback
join existing load/video/native/first-frame breadcrumbs. Native code reuses
the existing append/write/close trace for 6+ as well as 4/5: EGL resolution,
thread creation and its error code, app-main entry, framework context and app
context acquisition. The host still rotates to `tpk-etapas-anterior.txt` and
copies previous traces into the next session log. No new logging service.

## Validation and limits

- `bash tests/i612.sh`: production EGL loader/callback execution, persisted
  failure/success trace, API9 manifest/project, packaging flags and entry
  breadcrumb checks. Failed before the patch, passes after it.
- `bash tests/i612-build.sh`: full real Tpk65 release host compiled against
  cached API9 references, no restore/download/signing; TPK script syntax check.
  Existing unreachable-code/unassigned-field warnings remain.

No TPK-script dry-run mode exists. Full native ARM cross-build and package
assembly were not run: that path can download dependencies and reads local
build configuration. No network, SSH, GitHub, install, push, or deploy was used.
Pending: real QE65QN91B launch trace, exact installed artifact, ARM TPK build
and binary dependency verification. A TV trace must distinguish NUI/loader/
IME/EGL/app startup before closing #612 as resolved.
