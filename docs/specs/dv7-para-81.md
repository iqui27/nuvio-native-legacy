# Dolby Vision profile 7 -> 8.1 (not implemented)

Status: spec only. Requested by an LG C5 user who converted DV7 remuxes to 8.1
offline and got Dolby Vision with a few seconds more load time.

## Today

- `src/video.c` `dvPronto()` sends only single-layer profile 5/8 MKV (BL + RPU,
  no EL) to the Starfish BUFFERSTREAM path. Profile 7 (`dvMkvEl`) logs
  `profile 7 (el=1): stays on the TV player`, and the TV's URI player shows HDR10.
- `src/dts/dts_engine.c` (DV mode) demuxes, runs `hevc_mp4toannexb` and passes
  RPU NAL units through untouched; `AVDOVIDecoderConfigurationRecord` (line ~650)
  becomes the DolbyHdrInfo the TV gets.

## Where it would hook

1. `dvPronto()`: also accept profile 7 (BL + EL + RPU) when the converter exists.
2. The engine's video packet loop, after `hevc_mp4toannexb`, per access unit:
   - drop NAL type 63 (EL video, single-track P7 MKV); a dual-track P7 MKV just
     never selects the EL track;
   - rewrite NAL type 62 (RPU) to profile 8.1: libdovi
     `dovi_parse_unspec62_nalu` -> `dovi_convert_rpu_with_mode(rpu, 2)` ->
     `dovi_write_unspec62_nalu` (what `dovi_tool convert --discard -m 2` does).
     FFmpeg's public API cannot do this: its RPU encoder is internal (`ff_dovi_*`)
     and the `dovi_rpu` BSF only strips or recompresses.
3. Config record sent to the TV: `dv_profile = 8`, `bl_compatibility_id = 1`,
   `el_present = 0`, `rpu_present = 1`.

## Cost

- libdovi (Rust, MIT, C API) cross-built for armv7 webOS and the Tizen .tpk
  toolchains: ~1-2 MB in the binary, a new Rust step in `tools/arm.sh`.
- CPU: RPU parse/rewrite is microseconds per frame; NAL scan is one pass over
  each packet the engine already copies. Bitrate drops (EL gone).
- Work: ~1-2 days code plus TV time on a C9 (webOS 4) and a C4/C5.

## Which TVs need it

- LG webOS 4+ (DV path gated by `dvVersaoLiberada`): yes, P7 gets DV instead of HDR10.
- Samsung (Tizen): no, no Dolby Vision.
- Android TV: only devices whose decoder rejects P7 (most non-Shield). ExoPlayer
  would need its own extractor-level rewrite; separate work.

## Risks

- A wrong RPU rewrite shows wrong tone mapping or flashing; no fallback from
  inside the TV pipeline. Needs a known-good sample set (MEL and FEL).
- FEL titles lose the enhancement layer (12-bit detail); MEL is near lossless.
- Seeks must restart the NAL rewrite at the next IDR (stateless per AU: fine).
- The BUFFERSTREAM path already has the 2-crash kill switch
  (`dts-conversao.txt`), so a crash turns DV MKV off on that TV rather than looping.
- More start time: probe plus libdovi init; the user measured "a few seconds".
