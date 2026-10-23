# webOS Rust mkvcore ARM ABI activation

2026-10-19, branch `agente/205-mkvwebos`, round 2. The installed `armv7-unknown-linux-gnueabi` target links cleanly with the webOS SDK's `-mfloat-abi=softfp` objects. `tools/arm.sh` now defaults `NUVIO_RUST_MKVCORE` to 1 in its shared build path, covering every mode; `=0` keeps C and does not invoke Cargo. No extra target features or ABI-check suppression are needed.

The previous `armv7-unknown-linux-gnueabihf` target failed with `uses VFP register arguments`. The link gate now uses `armv7-unknown-linux-gnueabi`:

```sh
CARGO_NET_OFFLINE=true bash tests/mkvcore_webos_abi.sh
CARGO_NET_OFFLINE=true NUVIO_RUST_MKVCORE=0 bash tools/arm.sh --alto-cache --build
CARGO_NET_OFFLINE=true bash tools/arm.sh --alto-cache --build
```

All three commands completed with rc 0. SDK objdump reports maximum required version `GLIBC_2.12` for both product binaries. The C binary is 57,987,068 bytes; Rust is 58,191,988 bytes (+204,920). SDK nm reports eight `T nv_mkvcore_*` symbols in the Rust binary and none in C. Both builds use the same SDK, high-cache mode, P2P, libass and DTS configuration.

An archive of `integracao/2.0.5` reproduced the host harness failures before these changes: `mkvcore_paridade.sh` linked duplicate `mkv_tracks_alem`; subtitle harnesses missed the `i18n` test double; `video_url.c` missed the `dados_gravar` test double; the text-track choice harness also missed `legenda_pre_descartar`. Mechanical harness fixes add the missing doubles and symbol rename. `mkvass.sh` and `mkvass_recusa.sh` now honor the existing `NV_RUST_MKVCORE` test flag when linking `src/mkv.c`; the other standalone subtitle tests exercise their existing C subtitle parser.

Final host results: `tests/mkvcore_paridade.sh` and all 15 `tests/mkvass*.sh` scripts pass with rc 0 under both `NV_RUST_MKVCORE=0` and `=1` (32 commands). The C text-track harness was rerun after adding its missing double; final rc 0. `bash -n` and `git diff --check` also pass.

Raw baseline, build and per-mode host logs are in ignored `build/mkvcore-webos-proof/round2/`. Cargo ran offline. No installation, external network request, push, deployment or SSH was used.
