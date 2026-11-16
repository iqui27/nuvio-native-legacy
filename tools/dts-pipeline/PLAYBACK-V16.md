# Experimental playback v16

Based on v15 and its immediate-save player buffer-data toggle
(`6e5cb76f3625fa66f9b3eeeb30a11f1d95432630`).

The v15 TV log confirms fixed AAC was active. One session completed four audio
changes without another native Load. A second session fell back after 4.642 s
without confirmed presentation progress and disabled native Seek reuse for the
rest of that session. Large ready-byte counts sometimes accompanied only one
4 MiB contiguous interval at the requested position. The user observed choppy
moving video after switching, rather than a single held frame.

## Changes

- A nearby live DV range miss can recycle up to four existing slots to refill
  a contiguous gap in parallel. A full cache of distant future data no longer
  leaves just one slot repeatedly downloading the gap at one range per RTT.
  Most future payloads remain owned and cached. Active HTTP-owned slots are
  never repurposed, and the existing one-lane cancellation stays bounded.
- No extra range slots, payload budget, native player or copied video frame.
  The 64/128/256 MiB policy and memory protections are unchanged.
- After a DV audio change, keep the native player paused while feeding the
  existing bounded native queues. Resume when both audio and video reach two
  seconds beyond the switch target and the raw cache has a contiguous reserve
  of up to 16 MiB (scaled down for a small configured cache, or EOF).
- The hold has a six-second policy deadline, evaluated when the worker can
  run after reads. Existing transport deadlines still bound a blocked read.
  On expiry, resume through the existing native recovery path rather than
  waiting indefinitely. A user pause remains a user pause; cancellation and
  subsequent controls still work. No poster/loading-overlay changes.
- Native Seek progress checks exclude long engine reads from the renderer's
  four-second budget. An independent twelve-second post-preroll budget still
  forces recovery if progress never resumes. A genuinely refused/broken Seek
  still disables reuse for that session. No v13 changeAudioCodec API returns.
- Diagnostic logs report hold readiness/deadline, contiguous bytes, refill
  slots, recycled slots, and active/total no-progress time without media URLs.
- The nested player-buffer-data ON/OFF toggle saves immediately, independently
  of buffer-size confirmation; Back/Cancel preserves its saved value.

## Validation

Real HTTP regression: a full future cache and four missing earlier ranges,
each delayed by 800 ms. All four must arrive in under 2.5 s (serial refilling
takes at least 3.2 s), with exact patterned bytes, at least 12/16 old payloads
retained, and an unchanged memory cap. Existing cache, retry, cancellation,
short-response and 64/128/256 MiB tests remain, including ASan/UBSan.

Controller regressions assert Play is withheld until both streams and the
contiguous cache reserve are ready. Cases cover a large but discontinuous
buffer, user pause/resume, hold deadline, close while held, slow reads that
recover without a reload, and persistent missing clocks that still recover
within the absolute budget. Existing broken-renderer fallback tests remain.
The workflow also runs real MEL/FEL P7 conversion and fixed AAC/AC3 switches
across AC3 mono/stereo/5.1, EAC3, DTS and TrueHD, then builds the full ARM/P2P IPK.

## TV test

Keep the same heavy remux and 128/256 MiB preset. First select **DV fixed audio
(test) > AAC stereo**, close/reopen the movie, and switch AC3/EAC3, DTS and
TrueHD where available. A short held image should replace the choppy moving
image while the new track and contiguous reserve are prepared. Check sustained
playback after each switch and export QR logs if anything stalls.

Repeat with **Off (v12)** after reopening the source. Format/channel changes
can still require a native Load and a black transition in that mode. Fixed AAC
also converts native AC3/EAC3, adds CPU work, and does not preserve surround,
lossless audio or Atmos. Firmware behavior and physical black-screen duration
must still be confirmed on the TV; passing host tests cannot establish that.
