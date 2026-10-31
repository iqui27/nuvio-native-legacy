# Experimental playback v15

Based on v14 (`98ed174a43763b16c288d921b78d4655b4281bc3`), retaining v12's
native adapter and the opt-in fixed DV audio output experiment.

## TV log findings

The supplied v14 diagnostic export records 16 prepared revisions: 13 actual
native Loads and three reused native Seeks, all three confirmed by subsequent
presentation progress. Output formats vary between native AC3 mono/stereo/5.1
and converted AAC stereo. There are no `dv-fixed-audio` records, so the trace
does not demonstrate the fixed-output mode running on that source. The new
`audio-policy` record explicitly reports requested/effective mode and build gate.
The real settings save/reload path for this preference is now tested as well.

Audio changes can seek back to an earlier keyframe outside the active range.
The previous missing-range path canceled the entire speculative queue, even
when it held about 240 MiB of useful future data. Subsequent HTTP reads sometimes
waited 7–19.5 seconds; one direct request took 15 seconds before addon fallback.
During that event available RAM was about 363 MiB. This is evidence of missing
range/transport waits, not evidence of RAM exhaustion. Priming duration is not
a measurement of physical black-screen duration.

## Buffer changes

- On a nearby live DV cache miss, reuse one idle/queued/completed range slot
  instead of flushing the entire queue. Never repurpose a slot owned by an
  active HTTP worker. No additional payload buffer, range slot or native
  player is allocated.
- Retain existing future payload allocations. Refill the missing gap without
  overlapping retained requested/ready intervals; short HTTP prefixes remain
  exact and do not skip bytes.
- If every HTTP lane is occupied by speculative reads, request cancellation
  of only the farthest active interval to release a lane for the needed
  keyframe. Never reuse that worker-owned slot before completion. A real HTTP
  regression blocks all four lanes for eight seconds and verifies keyframe
  retrieval in under five seconds with exactly one canceled interval.
- Preserve the existing live awaited-range retry/timeout policy across DV
  seeks and audio changes. Startup, distant seeks, SDR/DTS and remux retain
  their existing paths. Provider stalls and the existing final retry can still
  take time; this is not a guarantee of immediate switching.
- Keep the v12 native Seek/paused-frame path. The optional **DV fixed audio
  (test)** choice from v14 is still needed to keep output parameters identical
  across format/channel changes. Off retains native passthrough and can still
  require a fresh Load/brief black transition. Refused or stalled native Seeks
  keep the existing recovery Load. No v13 `changeAudioCodec` call is restored.

## Player buffer text

The buffer-size editor now has a bottom **Show buffer data in player** toggle
(Polish: **Dane bufora w playerze**), default off. Remote and pointer ON/OFF
changes save immediately and stay active after Back/Cancel, without Confirm.
The size choice still requires confirmation; restoring defaults resets both.
A failed toggle save restores its previous state. It stays local to this
TV. It controls only the numeric `Buffer: ...` text, not buffering, the buffered
progress range or the separate green-button RAM overlay.

## Validation and TV test

Real HTTP tests at 64/128/256 MiB verify exact bytes and retained allocation
ownership when a preceding range is missing, including repeated gaps, short
responses, cancellation/retry and ASan/UBSan. The 256 MiB fixture retained
60/61 downloaded payloads, replacing one, within the unchanged allocation cap.
Real MEL/FEL tests cover fixed AAC/AC3 output across AC3 mono/stereo/5.1, EAC3,
DTS and TrueHD, repeated seeks and switches, and P8.1 RPU retention.

For the black-screen test select **DV fixed audio (test) > AAC stereo**, then
close/reopen the movie. This also re-encodes native AC3/EAC3, adds CPU work and
does not preserve lossless/Atmos/DTS:X audio. Keep the same 128/256 MiB preset
and source as the v12 comparison. Test switching and continuous playback;
export QR diagnostics if an issue occurs. Look for `audio-policy`,
`dv-fixed-audio`, `range-reuse`, and `nativeReload` to distinguish active policy,
preserved cache and actual video reloads. Physical-TV stability and black-screen
behavior still require confirmation.
