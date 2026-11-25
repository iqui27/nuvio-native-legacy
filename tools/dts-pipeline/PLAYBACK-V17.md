# Playback v17 — separate branch without QR log export

Based on the tested v16 playback implementation, including the follow-up removal
of redundant Confirm/Cancel buttons from the buffer-size editor. DV conversion,
DTS/TrueHD decoding, native seeking, frame hold, buffered seeking and RAM limits
are unchanged from v16. The build verifies those source files byte-for-byte,
except the read-only TrueHD notice getter, which now checks the prepared track.

- Rename **DV fixed audio (test)** to **Dolby Vision audio output**. Modes are
  **Original audio**, **AAC stereo**, and **Dolby Digital 5.1**. Preserve stored
  selections and the original-audio default. The help text explains lossy
  conversion and loss of lossless/Atmos/DTS:X information, and reopening video.
- Repeat the TrueHD conversion notice after selecting another track and
  returning to a converted TrueHD track. Wait for prepared playback before
  showing it. Stable demux track identity prevents repeats after seeking,
  temporary missing metadata, pause or a reload of the same track. DTS notices
  keep their existing once-per-source behavior.
- Below **Show buffer data in player**, add a RAM overlay ON/OFF control and
  a separate green-button shortcut switch on the same line. Both new settings
  default to **Off**, save immediately, and stay local to this TV. Buffer data
  remains independently controllable; its saved preference is preserved.
- When enabled, the green shortcut (or desktop F2) toggles the saved RAM overlay
  preference. Disabling the shortcut does not disable an already enabled RAM
  overlay. Key repeat does not toggle repeatedly. Failed saves retain the old
  state. Restore defaults resets the size and all three overlay preferences.
- Remove only the diagnostic snapshot export through local QR: its blue/F10
  action, QR card, snapshot/redaction module and HTTP download endpoint. Normal
  log viewing/sending and existing phone text-input/login QR features remain.
  Preserve bounded nonblocking HTTP writes used by normal phone text input.
- Build the complete ARM/P2P IPK. The original v16 branch remains available.

Validation includes real settings persistence, default-off/device scope,
keyboard and pointer navigation, independent toggles, restore and write failure;
actual TrueHD notice rendering and green-key handling; normal phone input over
HTTP; static absence of QR diagnostic export; and the v16 buffering, seeking,
real MEL/FEL DV conversion and audio switching regression suites.

The TV-confirmed v16 AAC mode remains available unchanged. AAC converts audio
to stereo; AC3 output uses a fixed 5.1 layout and does not create surround
content from stereo. Original-audio format changes can still reload video.
