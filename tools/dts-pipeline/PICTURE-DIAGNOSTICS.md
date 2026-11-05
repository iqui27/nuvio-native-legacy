# HDR10 / DV8 picture comparison experiment

The `dts-hdr10-dv8-test-2.0.8.6` build adds read-only numeric diagnostics under
the existing `NV_HTTP_BUFFER_DIAG` compile flag. It does not change routing,
seeking, audio conversion, native Load/Feed arguments or the HDR10+ experiment.
No source URL, raw video, thumbnails, decoded pictures or pointer addresses are
added to the export.

## Capture on LG

Use the same source and DTS track throughout. Start in the reproducibly broken
10–12 minute region, seek to the working 17 minute region, then return to the
same broken timestamp. Record whether the image is broken at each point and
export Diagnostics QR immediately after the sequence. Repeat separately for
HDR10 and DV8. A log alone cannot identify visible corruption.

## New records

- `video-demux`: packet position, key/corrupt flags, stream time base, byte count
  and fingerprint of container codec initialization and packet NEW_EXTRADATA.
- `hevc-structure phase=demux`: before the Annex-B filter, with the hvcC NAL
  length size where applicable. This is the selected video packet, not a raw
  network chunk. MP4 experimental pairing can already have joined its layers.
- `hevc-structure phase=engine`: after Annex-B and optional P7 processing.
- `hevc-structure phase=native-copy`: the actual pending copy supplied to native
  Feed, after the HDR10+ filter if enabled. The existing `picture-diag` record
  reports whether Feed accepted that observation.
- `video-timing`: adjacent PTS/DTS differences, duration, missing timestamps and
  backward timestamp counts over the first 120 packets. B-frame PTS reordering
  is normal and these counters must not be treated as decoder errors.

`nalMap` entries are `type/layer/temporalIdPlus1;bytes;hash;id;reference`, comma
separated. Unknown / inapplicable syntax fields are `-1`. SPS entries reference
VPS, PPS entries reference SPS, and slice entries reference PPS. PPS dependent
slice, extra header bits, tile and WPP flags provide a small syntax comparison.
Only a bounded prefix of base-layer syntax is interpreted; `syntaxUnknown=1`
does not mean that the complete picture is invalid. The last observed SPS/PPS
IDs and first slice PPS are summarized; use the map for duplicate definitions.

VCL, parameter, RPU and SEI fingerprints are separate and exclude the NAL
start code / length prefix. Match phase, PTS and packet ordinal, not epoch alone:
engine, worker and native reload lifetimes have independent epoch counters.
Demux packets may not correspond one-for-one after P7/MP4 layer processing.
In-band parameter insertion and filtered metadata can change the whole-packet
hash while VCL payloads remain identical. Hash equality is a diagnostic hint,
not a collision-proof comparison or software decode validation.

## Bounds and limits

Keep the existing six packet observations per open/seek (1–4, 24, 120), and at
most 120 inexpensive timestamp updates per engine/demux epoch. Each structure
scan inspects at most 2 MiB / 256 NALs and its numeric map is capped at 640 bytes.
`clipped`, `mapClipped`, `malformed` and `syntaxUnknown` make partial observations
explicit. All inspection borrows existing bytes; the syntax prefix uses a
256-byte stack buffer, with no video allocation, persistent packet cache or
background task. Two small timing structs and a counter are compiled into an
engine only with diagnostics enabled. Normal builds omit the hooks.

The records cannot prove the original container, complete video bitstream,
decoded image or LG hardware decoder correct. They narrow the next experiment
by separating transport/copying, format conversion, header selection and timing.
