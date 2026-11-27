# Playback v22 — Dolby Vision MP4 integration without QR log export

Base: v17-ui, commit `85ca963f28afb9ddd0edded0e013fca579493fbc`.
Branch: `build/playback-v17-no-qr`, integrating the validated MP4 test 4 (v21)
playback changes and the matching Dolby Vision opening mark.
The separate `build/mp4-bd-remux-test` branch retains its diagnostic QR build.

## Scope and settings

Settings > Playback > this TV's sound/video section:
**Dolby Vision MP4 (Experimental)**, default **Off**, stored on this TV only.
Reopen the video after changing it. Old preferences retain their IDs and keys.

Off retains the v17-ui playback policy. On retries a native startup demux error
204 only for a source recognized as MP4, once per opening. A playing source,
MKV, live source and an already attempted converter do not enter this retry.
The worker then verifies the real container metadata: exactly two HEVC video
tracks, one HDR10-compatible base and a separate P7 EL/RPU track. Unrelated
MP4, P5, ambiguous tracks and encryption are rejected. A single-track P8.1
source is also accepted only with version 1.0, compatibility ID 1 (or the
strictly verified ID-0 recovery described below), BL/RPU
present, EL absent, verified HDR10 base metadata and a valid first P8 RPU.
Its metadata passes through unchanged; the P7 converter is never invoked. A BD REMUX filename
alone is never evidence of this layout. The normal MKV P7 preference remains
independent; enabling this explicit test allows P7 for this MP4 retry only.

The retry pairs compressed base pictures with the separate track's RPU by
both DTS and PTS. Existing libdovi mode-2 conversion produces P8.1 metadata;
EL pictures are discarded without decoding, and base video is not re-encoded.
The existing audio selection/fixed-output, seeking and native reload fallback
remain in use. Missing/invalid RPU, timestamp mismatch, excessive interleave
or cancellation fail explicitly rather than advertising unverified DV output.

## Memory

No additional video decoder, native player instance or HTTP lane is added.
Pairing retains at most eight base packets sharing a 4 MiB payload budget and
64 RPUs of at most 8 KiB each. Four existing HTTP range slots (about 16 MiB)
are reserved before workers start, covering pairing storage and temporary
packet growth without raising the previous read-ahead payload budget. This
reserve applies only to the new MP4 engine, not MKV or normal HTTP buffering.

## Diagnostics

The normal on-TV log panel remains available with the red remote button.
Diagnostic QR export, downloadable snapshots and the report HTTP endpoints
are absent in this version. Ordinary phone text-input/login QR features are
retained. Their source files match the previous no-QR branch exactly.

New numeric/technical log entries use `[mp4-bd-test]` and `MP4 test:`. They
identify the native 204 trigger, verified BL/EL indices, P7 -> P8.1 output,
pair alignment and bounded failure reasons without exposing stream URLs.

Test 2 reproduces the TV's initial configuration rejection using a 23-byte
hvcC and no MP4 colr box. The minimal FFmpeg HEVC parser reads dimensions but
does not publish SPS VUI colour fields. For verified separate-track P7 only,
missing HDR fields are recovered from a bounded base SPS read (hvcC arrays or
in-band packets), without decoding pictures or assuming unknown colour is HDR.
The probe is limited to 64 packets / 4 MiB and an 8 KiB SPS, within the same
16 MiB reservation. An in-band probe rewinds before audio setup/resume so the
opening pictures and audio are retained. Missing SPS, explicit SDR, colour
conflicts and invalid headers still fail. Logs now give individual BL/EL
parameters, specific rejection reasons and native numeric errors even if the
backend JSON does not carry a useful error description.

## Validation and TV testing

Real FFmpeg + libdovi tests cover synthetic MEL/FEL MP4, reordered B-frame PTS,
both track orders, complete and 23-byte/in-band HEVC configuration, repeated
seeking and audio switching with Original/AAC/AC3 output, bad/missing RPU,
timestamp mismatch and cancellation. They also re-run the existing real MKV
and single-track P8 MP4 regression fixture with the feature compiled in.
ASAN/UBSAN, actual webOS callback routing, settings default/scope/persistence,
normal HTTP buffering, diagnostic-export removal and ordinary phone-input
HTTP tests are included in CI.

On TV compare the same failing 117 GB seleZen MP4 with the new option Off and
On, reopening the movie each time. Prefer a 128/256 MiB setting for a heavy
remux. Check actual Dolby Vision activation, sound, seeking and audio changes.
Also verify known-good MKV and native P5/P8/HDR10 MP4 with the option On.
The synthetic fixture tests validate routing and packet integrity; they do
not establish playback or picture quality on a physical LG TV. Also test the separate 46.91 GB P8 release; recovery requires its actual
compatibility ID to be 1 and valid P8 metadata, rather than guessing from
the filename.

## Test 3 seek/resume corrections

A MOV demuxer can seek the EL track to a much older sync sample than the BL.
Obsolete RPUs then fill the 64-entry queue before its matching base picture
arrives. Reopening at the saved position repeats this failure. The dual-track
reader probes the actual first base keyframe's DTS/PTS, repeats the identical
seek before output, and discards only EL samples strictly older than that
verified anchor. The first BL after replay must match the anchor exactly.
Probe/discard work is limited to 1024 packets / 8 MiB; base packets retain their
4 MiB limit, exact DTS/PTS pairing and all existing queue limits. No video is
decoded and the HTTP read-ahead allocation is unchanged.

For opt-in MP4 sessions only, consecutive seek commands retain the latest
requested target while the first accepted native seek awaits a valid clock.
The existing 4/12-second no-progress deadlines and reload fallback remain in
force. Pause, cancellation and audio selection retain bounded behavior. Other
playback sessions retain the v17-ui controller policy.

The real-engine regression has a deliberately coarse EL sync index and over
64 old RPUs. It covers saved-position startup, repeated forward/back seeks,
audio selection, reopening, all three audio outputs, decoded HEVC, exact RPUs,
monotonic DTS and the unchanged memory capacity. P8 tests compare every RPU
byte and decoded frame hash, reject unsupported metadata, and exercise
64/128/256 MiB presets. The native seek controller has separate rapid-command,
feature-off, timeout, pause, stop and audio tests.

## Buffer comparison with v17-ui

The cacheboost/moviebuffer implementations, pipeline/adapters, HTTP range
transport, source readers, memory guards and refill policy are unchanged.
This MP4 recovery reserves four existing 4 MiB prefetch slots (about 16 MiB)
for bounded metadata work. Conversion diagnostics count prefetch slots plus
one active read block; the reported capacity is therefore approximately:

| Selected preset | v17 conversion path | MP4 recovery |
| --- | --- | --- |
| 64 MiB | 68 MiB | 52 MiB |
| 128 MiB | 132 MiB | 116 MiB |
| 256 MiB | 260 MiB | 244 MiB |

For `240/244 (256 selected)`, 240 is current buffered payload, 244 is capacity,
and 256 is the preset. This is not total application RAM. Regular HTTP RAM
buffering and its safety reductions remain separate and unchanged.

On TV specifically resume the previously failing 117 GB movie near 3835 s
(about 1:04), seek forward/back, close/reopen, then test successive seeks and
audio changes. Inspect the on-TV log panel if any step fails.

## Test 4: missing P8 compatibility ID and startup text

The 46.91 GB TV log identifies one P8 HEVC video track with BL/RPU present,
EL absent, a 23-byte hvcC, unspecified colour fields and compatibility ID 0.
Test 3 stopped at the ID guard, before checking the actual SPS or RPU.

The opt-in recovery now permits an ID-0 candidate to proceed only if its
actual SPS proves 10-bit 4:2:0 PQ/BT.2020 with a non-constant BT.2020 matrix,
matching dimensions and no explicit container colour conflict. This SPS check
is mandatory even when the container claims HDR10. A valid first P8 RPU is
also required. Only after both checks pass does output signaling use ID 1.
The original input configuration, picture packets and RPU bytes are retained.
The existing 64-packet/4 MiB probe limits and 16 MiB reserve are unchanged.
Explicit non-HDR10 compatibility IDs, SDR, HLG, missing SPS, missing/corrupt/P7
RPU, ambiguous tracks and encryption remain rejected. P7 pairing/seek behavior
is unchanged. Logs explicitly distinguish input ID 0 and verified output ID 1.

Real FFmpeg tests include empty-hvcC/in-band recovery, all three audio outputs,
all buffer presets, seek/resume/audio/reopen, source configuration preservation,
byte-identical RPU and identical decoded picture hashes. Actual encoded SDR
and HLG samples are refused even with false HDR10 container tags. The
verification flag is reset before another source opens; OFF retains ID 0.

The MP4 fallback now uses the existing startup text slot for Reading the file,
Opening source and Preparing the image, based on actual backend state. It does
not activate the MKV screen's input/mute controls or change the opening art.
The text ends after the first presented picture and cannot return on seek or
audio reload. Renderer tests cover both opening layouts; the real webOS routing
fixture covers startup, native/OFF isolation, errors and post-startup hiding.
The option help is reduced to three short sentences. On TV retest both the
working 117 GB P7 remux and the 46.91 GB P8 file with this option enabled.

## V22 opening mark

The existing DOLBY VISION mark above the title now also appears during MP4
recovery startup, in the same center/corner position with the same accent glow
and decorative rules. Its accent lights when the source is validated. The
mark uses the existing opening-only gate, disappears after the first picture,
and cannot reopen the artwork during seeking or audio changes. Renderer tests
exercise both layouts, subdued/verified states, hiding and the unchanged MKV
presentation. Playback logic, buffering and memory guards remain unchanged
from the validated MP4 test 4 integration.
