# Experimental playback build v14

Based on the TV-tested v12 tree (`add9152b778c4e8a2510ee23c6899334cece1dfa`).
Removes v13's `changeAudioCodec` experiment after reported video/buffering
stalls and application hangs. Host tests cannot establish LG firmware stability.

## Optional fixed DV audio

Settings > Playback > More options > Sound and video on this TV >
**DV fixed audio (test)**. Applies when opening the next source:

- **Off (v12)** (default): native AC3/EAC3 passthrough and the existing DTS/
  TrueHD conversion preference. Format/channel changes can reload the player.
- **AAC stereo**: convert all supported audio on the DV packet path to AAC-LC,
  stereo, 48 kHz, 192 kbit/s.
- **Dolby Digital 5.1**: convert to AC3, six-channel output, 48 kHz, 640 kbit/s.
  Stereo inputs retain stereo content; this does not restore surround channels.

This deliberately trades passthrough for a stable decoder configuration.
Audio is re-encoded, adding CPU work; lossless audio, Atmos and DTS:X metadata
are not preserved. The separate DTS output preference remains unchanged and
still controls normal SDR/DTS playback. Native DV URI routing is unchanged.

The fixed format is selected before the first Load and retained across track
changes, seeks and recovery. Each switch still uses the exact selected source
track and v12's native Seek, pause/prime and presentation-clock guard. There is
no runtime native audio-codec reconfiguration, second player or extra packet
queue. HTTP buffer limits, memory safeguards and native queue limits remain
v12's. Existing refused/stalled-Seek fallback can still reload and briefly
interrupt the image; this is not a guarantee of seamless playback on firmware.

The setting is appended to persisted IDs, is off on upgrade and stays local
to this TV, outside account sync/device backup. Non-experimental builds ignore
it. Missing requested AC3 encoder fails explicitly rather than silently
switching the selected output policy.

## Tests

- Controller tests: stable AAC/AC3 output, repeated switching with one native
  Load, paused switching, invalid/rejected-start options, SDR isolation and
  refused Seek fallback; all existing v12 stall/cancellation/seek tests.
- Real FFmpeg/DV7 tests: MEL and FEL fixtures, AC3 2.0/5.1, EAC3, DTS and
  TrueHD, both output modes, repeated switch/seek, P8.1 RPU retention, source
  metadata, unchanged bounded cache and cancellation.
- Settings: default-off, appended IDs, invalid value and local-only scope;
  QR export retains numeric conversion diagnostics and build identification.
- Full ARM IPK includes P2P and both original v12 firmware adapters.

## TV test

1. Use the same source and buffer preset as v12 (128 or 256 MiB for a heavy
   remux). First confirm **Off (v12)** still behaves like v12.
2. Select **AAC stereo**, close/reopen the movie, and switch repeatedly between
   AC3 2.0/5.1, EAC3, DTS and TrueHD where available. Check DV, sound, A/V sync,
   black screens and buffering. Test pause, seek and stop as well.
3. If using surround equipment, repeat with **Dolby Digital 5.1**, reopening
   the source after changing the setting.
4. Play continuously for several minutes after switching. TV stability and
   read-ahead throughput matter more than avoiding a brief black transition.
5. If an issue occurs, export QR diagnostics. The header identifies v14;
   `[dts] dv-fixed-audio` records source/output codec and channels. Native Load
   generations distinguish actual video reloads from metadata-only changes.

RAM overlay remains off by default and toggles with the green remote button.
