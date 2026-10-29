# Issue #558: Q50A black screen after 2.0.6

Reported device: Samsung QE32Q50AEUXXU, Q50A (2021), Tizen 6.0.
2.0.5 worked; reinstalling 2.0.6 did not help. No device log is available.
The matching package is NuvioTpk60 (API8). The exact installed artifact is
unconfirmed, so a package mismatch remains possible.

## Ranked causes

1. **New eager Samsung IME setup: strongest launch-path regression.**
   `6bd1c06ff`, retained by `e29de85cd`, adds `Voz.cs` to all three NUI hosts
   and constructs `Voz` in `Program.Arranca` (`tizen-tpk/Program.cs:306`),
   after `video-init` but before
   `nv_tpk_iniciar` and GL window creation. Its constructor
   (`tizen-tpk/Voz.cs:58`) previously called
   `Microfone` and `MontarCampo`, including native `TextField` construction,
   `Window.Add`, `GetInputMethodContext`, and a status subscription
   (`tizen-tpk/Voz.cs:95`).
   A managed exception was already caught; that alone does not explain black
   startup. A firmware native abort or a synchronous IME hang would bypass
   those catches and prevent both app UI and the later watchdog/log screen.
   API8 uses an older NUI/IME implementation and a separate GLWindow; this is
   a plausible compatibility boundary, not proof that this model's IME fails.
   The host test proves eager optional API access and its removal, not the
   reporter's firmware defect.

2. **Native-library load/resource failure after adding static FFmpeg.**
   The release also changes `tools/tpk.sh` to link FFmpeg into the 6+ native
   library. DTS playback calls in `Video.cs` run only when opening media, so
   they cannot directly explain a fresh launch. Increased library mapping,
   initialization, or device resource pressure could fail before first frame.
   There is no device loader error, memory measurement, or evidence linking
   this specifically to the Q50A. `dlopen-so` already has a managed error
   screen. The new first-frame fallback covers a responsive event loop with
   no app frame; a process abort or a blocked main thread still requires the
   persisted stage trace on the next launch.

3. **Wrong package / firmware-specific GL startup.**
   Confirm the installed file is NuvioTpk60, not NuvioTpk or NuvioTpk65.
   No GL window-mode change appears in the requested host release delta.
   Existing API8 first-frame accounting could hide a stalled native startup:
   `src/tpk.c:248` reported its temporary black clear as a successful
   frame. The watchdog tracked callback activity rather than actual first UI.

## Suspects excluded or downgraded

- No `tizen-tpk/*/tizen-manifest.xml` changes between the tags; no new release
  recorder privilege or startup permission request. The recorder privilege
  belongs to the separate `NvTextoCanario` build.
- `8a53efc0b` keeps host dimensions fixed at 1920x1080. It exports the model to
  a heuristic: `ui_size_model("QE32Q50AEUXXU")` yields 32 and
  `ui_size_inches(32)` yields 1.15. Unknown/invalid values fall back safely.
  It does not allocate a surface from physical panel dimensions. The optional
  model export now catches failures separately from required native startup.
- `163ee5b04` (TV log) and `2f444a501` (source HDR badge) are ancestors of
  v2.0.5. They are not regressions introduced by v2.0.6. Samsung startup in
  `src/main.c` reports unknown HDR capability; no new display HDR query runs.
- `e29de85cd` session guards affect requested keyboard/lifecycle work; its
  eager constructor was inherited from the microphone feature.

## Fix and executable evidence

`Voz` now registers callbacks with unknown capability without accessing the
Samsung IME or microphone feature. Only an explicit microphone request builds
the field and queries capability. Managed setup failure/null context cancels
once, cleans up the partial field, and permits retry. This changes Automatic
mode: before the first request it treats microphone capability as unknown.

The shared host (`tizen-tpk/Program.cs:692`) logs and shows its existing NUI
error screen after 15 seconds
without an app frame, even if native callbacks continue returning zero. A
native startup failure no longer silently closes before that screen. API8
still clears/swaps startup buffers, but those clears no longer count as app
frames. Existing startup exceptions continue using the existing error screen.

`bash tests/tpk_voz_host.sh` executes real `Voz.cs`, extracted unchanged host
watchdog/render/lifecycle methods, and the extracted native startup-clear
branch against host doubles. New cases failed before the patch and pass with
it: no eager IME/query, constructor failure, denied capability, null context
and retry, zero-frame timeout, failed native startup, and healthy first frame.
Existing opening/cancellation/lifecycle cases remain covered. The same command
compiles full release hosts against cached API8/API9/API11 and the text canary.

Final checks: `bash tests/tpk_voz_host.sh` returned 0; `bash tools/tpk.sh`
returned 0 and produced all four 2.0.6 packages in `build/tpk/`, with FFmpeg
and P2P enabled for 6+, plus the separate 4/5 library. Package contents and
native-library identity checks passed. The build reported missing server
configuration (`local.properties` absent), so these artifacts prove compilation
and packaging only and must not be published as configured release packages.
Existing compiler/SDK warnings remain. `git diff --check` returned 0.

No TV was installed or contacted. A host test cannot prove NUI rendering on
the reported firmware, prevent a native process abort, or recover a main
thread permanently blocked inside firmware or GL. Confirm with a Q50A tester
and the launch trace before claiming the reporter's root cause is established.
