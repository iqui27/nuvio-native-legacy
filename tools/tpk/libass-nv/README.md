# libass for the Samsung .tpk (libass 0.17.5 + patches)

Only the .tpk uses this. webOS, the .wgt and Android keep their own libass.

| Patch | What it does |
|---|---|
| `0001-neon32-bitmap-engine.patch` | ARMv7 NEON bitmap engine (`libass/arm/neon32.c`): rasterizer, blend, `\be` and the Gaussian blur stripes. It is a port of libass's aarch64 engine to NEON intrinsics. It keeps the C engine's exact integer arithmetic, so the output is bit-identical. It is selected in `ass_bitmap_engine_init` on 32-bit ARM with NEON, independent of `--disable-asm`, and checkasm gets an armv7 entry. |
| `0002-threaded-blur.patch` | The NEON blur stripe pipeline is written per stripe range, and big blurs are split across helper threads (`ass_nv_set_threads`, 1–4; the app uses 3). Each stripe depends only on the read-only source, so the output is identical. |
| `0003-nuvio-api.patch` | The app's API, at the end of `ass.h` (`ASS_NV_API`): `ass_nv_set_engine` (C or NEON, for the CI check), `ass_nv_set_threads`, and `ass_nv_set_dialogue_alpha` (the app's subtitle opacity for dialogue only: events without `\pos`, `\move`, `\clip`, `\iclip`, `\org`, `\pbo` or a drawing; signs keep the author's alpha). The blur stays libass's own, on the CPU (the GPU blur was tried on the S95C and dropped 27/10/2026: CPU with 3 threads was faster and lighter). |

`cflags` holds extra compiler flags for libass only (now `-O3 -mtune=cortex-a53`, plan Step 2). They are part of the stamp, so changing them rebuilds libass.

## How it is built
- `tools/tpk.sh` applies the patches on the host, to a fresh copy of the tarball in `$CACHE/src/libass-nv`. The build image has no `patch`.
- It writes the hash of the patches as `.nv-stamp`.
- `tools/tpk/deps.sh` builds a copy of that tree and records the same hash in `prefix/lib/.libass-nv`.
- Changing a patch rebuilds libass only.

## How it is checked
CI runs `verify.sh` in the same linux/arm image under qemu (see `.github/workflows/build-tpk.yml`):
1. libass's checkasm compares every NEON function with the C engine on random input. Odd seeds run with 4 threads and every job split.
2. `verify.c` renders `samples/*.ass` with C, NEON with 1 thread and NEON with 4 threads on a time grid, and compares every `ASS_Image` byte for byte. Last, it checks the dialogue opacity: at 255 the frames are untouched, at 127 only alpha bytes change, and only on dialogue (the all-`\pos` samples stay as they are).
