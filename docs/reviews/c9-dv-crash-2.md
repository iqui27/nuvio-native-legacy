# LG C9 Dolby Vision crash, second investigation

The new mappings identify an unchecked null HEVC timing/SPS context in LG's DV VES demuxer. The exact movie packets are unavailable, so the original bitstream trigger cannot be proved offline. The fix conservatively keeps profile 8.1 on its HDR10 base layer when our probe has not observed in-band RPU in a complete video frame. Confirmed-RPU sources retain the existing DV path. This is a bounded admission guard, not a claim that RPU presence proves every firmware input safe.

## Evidence

Owner report: "crash na c9 no dv no mesmo filme de novo". Device: OLED65C9PSA, webOS 4.10.2, firmware 05.50.00, app 2.0.7. Inputs are the already downloaded files under `/Volumes/ExternalSSD/tmp/rev/c9crash2/`. No TV access or network requests were made. The version reset at log line 1701 explains why the two-death guard admitted another attempt; it is not the faulting operation.

`nuvio.log:1806-1807` declares profile 8, level 6, RPU=1, EL=0, BL=1, compatibility=1. The complete 709-byte Tracks contains 129-byte hvcC without NAL 62 and one dvvC mapping. The 327680-byte probe saw **zero video frames**, with RPU=-1 and no located partial frame. This does not establish that the movie lacks RPU. Native Load reports DV=1/profile=8, E-AC-3 six channels; both feeds and Play are accepted before the fatal signal.

The crash report records SIGSEGV/SEGV_MAPERR, address `0x8a54`, PC `0xe550d2a2`, LR `0xe550a8a5`, thread `video-queue:src`. The executable mapping of `/usr/lib/libdv_ves_dmx_m.so` begins at `0xe5509000`, file offset zero. ELF PT_LOAD virtual address is also zero, so PC is `0x42a2`; LR is Thumb `0x18a5`, instruction address `0x18a4`. The earlier report lacked these mappings; its audio-thread hypothesis is superseded for this crash.

Firmware library SHA-256:

- `libdv_ves_dmx_m.so`: `dc7811d0b7a9e4924ad61a8eda6cbc1317d308d389fe9928a0b29a459fa7f1b3`
- `libgstdvsplitter.so`: `4096b724dbbaa624cf751a1e09cdc90a79a8130d94ef0b9680067a014ac04216`

Both libraries were treated only as data. Disassembly used the cached `nuvio-webos-sdk` image with `docker run --rm --network none --pull never`, a read-only evidence mount, and `arm-webos-linux-gnueabi-readelf` / `arm-webos-linux-gnueabi-objdump`. No firmware code was executed.

## Fault instruction and state

Relevant Thumb instructions (offsets within `libdv_ves_dmx_m.so`):

```text
4266  blx read_sei_tv       ; reads SEI payload type
426c  mov r5, r0
4274  blx read_sei_tv       ; reads payload size
428c  cmp r5, #1            ; picture-timing SEI
428e  bne ...
4290  ldr.w r3, [r7,#0x714] ; current timing/SPS context
4294  mov r2, r3
4298  add.w r4, r2, #0x6080
429c  movw r3, #0x2974
42a0  adds r4, #0x60
42a2  ldr r3, [r4,r3]       ; *(context + 0x8a54), no null check
```

`0x6080 + 0x60 + 0x2974 = 0x8a54`. The reported fault address therefore implies a zero context pointer at this instruction. `read_sei_tv` at `0x1690` implements the standard SEI byte sum with repeated 0xff extension bytes; payload type 1 is picture timing, not Dolby RPU/NAL 62.

The adjacent getters `hevc_parser_get_cts_offset_frames` (`0x40a4`), `hevc_parser_get_num_units_in_tick` (`0x40c4`), and `hevc_parser_get_time_scale` (`0x40d8`) use the same context pointer, with explicit null checks. `0x40b0-0x40b8` reads the same context field at `+0x8a54` when deriving CTS offset. Context entries are indexed with stride `0x8a6c`; the SPS parser reads the SPS ID at `0x49f2` and populates that table. The SEI payload-type-0 branch at `0x433a-0x4380` reads an unsigned Exp-Golomb SPS ID, validates the entry, then assigns the current pointer. `hevc_parser_reset` clears the parser memory before restoring callbacks. Picture timing can thus arrive while no current context has been selected. Exact C field names are unavailable; the timing/SPS role follows these instructions and getters.

LR `0x18a4` lies just after `memcpy` in the buffer reader (`0x1874-0x18ca`). It is not a call from the audio sink, nor a direct caller of the fault instruction. LR may reflect an earlier nested call; it is not a reliable backtrace by itself.

The splitter exports `gst_dvsplitter_chain`, imports `dv_ves_dmx_process/reset/init`, and carries `video/x-h265`, alignment, `dv_profile`, and Dolby track/profile cap strings. This corroborates the video queue's DV demux route. We did not capture the runtime GStreamer caps or packet bytes.

## Our stream contract and likely trigger

`video.c:lerMkv` copies the declared DV flags from dvcC/dvvC and previously admitted profile 5/8 with BL+RPU and no EL without requiring any inspected video frame. The diagnostic RPU probe was informational. `dvPronto` then enables `dtsModoDv`; `iniciarDts` hands this to `dts_playback_next_dv` and opens the separate BUFFERSTREAM worker.

`dts_engine_open` reads FFmpeg's `AV_PKT_DATA_DOVI_CONF`, validates the same layer/profile flags, and creates `hevc_mp4toannexb`. Both passthrough E-AC-3 and converted DTS routes send the filter output as compressed video. The filter preserves RPU NAL bytes; it does not synthesize missing RPU or HEVC timing state.

`adapter/starfish.cpp:load` sends codec H265, format RAW, provider Chrome, and `DolbyHdrInfo` with clear encryption, profile ID and single track. `feed` passes each buffer address, length and timestamps to Starfish. Our adapter does not set GStreamer caps directly or pass separate hvcC/dvcC/dvvC codec_data; LG builds its caps from the native Load contract. The actual video input is Annex-B after filtering, not length-prefixed Matroska samples. Thus changing a dvcC versus dvvC caps name in our code would not address the observed dereference.

The **locally cached ARM FFmpeg 7.1.5 source** at `/tmp/nuvio-dts-build/ffmpeg-7.1.5/libavcodec/bsf/hevc_mp4toannexb.c` inserts hvcC parameter sets only immediately before the first IRAP NAL of each packet. Leading prefix SEI is emitted before those inserted sets. Picture-timing SEI before established timing/SPS state is a credible trigger consistent with the fault. Missing buffering-period initialization, reset/start position, or rejected SPS are also possible. Prepending VPS/SPS/PPS alone would not demonstrate that the firmware's active timing pointer is initialized; no speculative reorder or firmware interposition was added.

Missing in-band RPU is **not proved** and is not the instruction's direct input. We use the existing probe's unknown/absent RPU result as an intentionally conservative admission boundary, as permitted by the task. Zero inspected frames must no longer count as a safe profile 8.1 source.

## Minimal fix and regression

After the existing probe, profile 8 with compatibility ID 1 and unconfirmed in-band RPU (`rpu != 1` or zero complete frames) clears `dvMkvRpu`. Probe completion (`dvSondado`) is published only after that check, preventing the main loop from admitting the declaration before the guard runs. The existing shared `dvPronto` refusal then keeps the native URI HDR10 playback, rewinds the covered interval to the requested entry position, and exits the DV opening screen. No DV BUFFERSTREAM Load is attempted. Log:

```text
[dv] HDR10: profile 8.1 in-band RPU unconfirmed (rpu=-1 frames=0)
```

The guard does not rewrite the file, change persisted settings, reset death counters, or disable all DV. Profile 5 retains its existing behavior because its base layer is not HDR10. Confirmed-RPU profile 8.1 retains DV. Other profile/compatibility combinations retain their existing decisions.

`tests/dvsonda_contentor.sh` compiles the real webOS `video.c` branch on the host, with only the external bus/stream startup and downloaded probe results substituted. The added cases include the exact reported metadata, a complete frame without RPU, hvcC NAL 62 without in-band proof, confirmed in-band RPU, profile 5, and a later unconfirmed source following a confirmed one. Assertions cover admission/refusal, retained native playback, rewind to 600 seconds, and opening-screen exit. Existing MP4/TS and network-retry cases remain.

Before the production change, the exact crash-input case failed the `dvPronto()` assertion (rc 134). After the change, the same command passed (rc 0).

## Final validation

- `bash tests/dvsonda_contentor.sh`: rc 0 (regression was rc 134 before the fix). A final rerun initially exceeded the 60-second tool limit during compilation; the completed rerun passed.
- `bash tests/i18n.sh`: rc 1, existing Cyrillic Serbian translation in `src/idioma_sr.h:3806` triggers 15 script violations. The same line exists in the starting HEAD; this change does not edit translations.
- `bash tools/build-local.sh`: rc 0.
- `bash tests/c9_dv_arm_offline.sh`: rc 0. This invokes `tools/arm.sh --build` through a temporary Docker wrapper forcing `--network none --pull never`, with Cargo offline. Full ARM binary and both Starfish adapters built; FFmpeg, libass, Rust MKV core and P2P remained enabled. Existing compiler/linker warnings and empty local service credentials remain.
- `git diff --check`: rc 0.

## Limits and remaining proof

The supplied movie was not downloaded or replayed; no TV run was authorized. Offline proof establishes the null-context fault mechanism and rejection of the reported probe state. It cannot prove the movie's precise NAL ordering, reproduce proprietary firmware execution, or guarantee every confirmed-RPU stream is safe. Some valid profile 8.1 sources whose first frame lies outside the bounded probe now use HDR10. A future packet-level admission check would be needed to recover those sources safely. Device playback verification remains pending; nothing was pushed, deployed or installed.
