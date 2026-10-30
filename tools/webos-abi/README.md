# webOS runtime ABI baseline

The webOS linker guard checks **undefined dynamic symbols**, including weak
imports, immediately after linking `nuvio-proto.arm`. It rejects SDL imports
outside `sdl-2.0.4.txt`, GLIBC versions above 2.19, and GLIBC_PRIVATE. It does
not change Android, Tizen, desktop or iOS builds. Run the portable regression
check with `bash tests/webos_abi.sh`.

## Sources and generation

`sdl-2.0.4.txt` contains the 565 unique `SDL_DYNAPI_PROC` names from the official
[SDL release-2.0.4](https://github.com/libsdl-org/SDL/blob/release-2.0.4/src/dynapi/SDL_dynapi_procs.h),
commit `e2fd1c0fe39d83ecdee1e0c6359a299f0afad13b`. Only official release tags
were fetched, with depth 1, from https://github.com/libsdl-org/SDL.git.
Extraction: match `SDL_DYNAPI_PROC\([^,]+,(SDL_\w+),`, deduplicate, sort, one
name per line. Platform-specific declarations are retained; this is an upstream
API-age guard, not a complete firmware export allowlist.

Official dynapi history places these known crash symbols at:

| Symbol | First SDL release | Existing replacement |
| --- | --- | --- |
| SDL_CreateRGBSurfaceWithFormat | 2.0.5 | `nv_superficie`: PixelFormatEnumToMasks + CreateRGBSurface |
| SDL_RWseek | 2.0.10 | `rw->seek(rw, ...)` |
| SDL_GetTicks64 | 2.0.18 | `nv_ticks_ms`, monotonic clock |

The SDL 2.0.4 ceiling is the requested policy. Some webOS 3.0–3.4 firmware
actually has SDL 2.0.2; therefore the final imported SDL set was also compared
against the W16N and W17H firmware exports. A clean upstream guard alone does
not prove every SDL 2.0.4 function exists on every webOS 3 TV.

Library versions below come from retail `info.json`, `packages.json` and library
symbol JSON in [webosbrew/dev-toolbox-cli common/data](https://github.com/webosbrew/dev-toolbox-cli/tree/main/common/data).
These are observed firmware versions, not guarantees about every original or
future firmware. LG package names sometimes misreport the upstream SDL version;
[webOSbrew documents the SDL 2.0.2 override for dreadlocks](https://www.webosbrew.org/develop/guides/library-version/).

| webOS / sample | SDL2 | glibc | curl | OpenSSL / SSL SONAME suffix |
| --- | --- | --- | --- | --- |
| 3.0–3.4 / W16N 3.4.0 | 2.0.2 (package says 2.0.0-78.16) | 2.19 | 7.40.0 | 1.0.1p / .1.0.0 |
| 3.5–3.9 / W17H 3.9.2 | 2.0.4 | 2.19 | 7.50.1 | 1.0.2k / .1.0.0 |
| 4.0–4.4 / W18R 4.4.2 | LG 2.0.5 package | 2.24 | 7.53.1 | 1.0.2p / .1.0.0 |
| 4.5–4.10 / W19P 4.10.0 | LG 2.0.5 package, .0.4.1 file | 2.24 | 7.53.1 | 1.0.2p / .1.0.0 |
| 5.x / W20L 5.3.1 | 2.0.10 | 2.28 | 7.61.0 | 1.0.2p / .1.0.2 |
| 6.x / W21P 6.4.0 | 2.0.10 | 2.28 | 7.61.0 | 1.0.2r + 1.1.1t / .1.0.2 + .1.1 |
| 7–8 / W22O, W23O | 2.0.14 | 2.31 | 7.69.1 | 1.1.1t / .1.1 |
| 9 / upgraded W23O | 2.0.14 | 2.35 | 7.82.0 | 3.0.9 / .3 |
| 10 / upgraded W22H | 2.0.14 | 2.35 | 7.82.0 | 3.0.13 / .3 |
| 11 / upgraded W23O | 2.0.14 | 2.39 | 8.7.1 | 3.2.3 / .3 |

For webOS 4, the owner's C9 library is the actual export reference:
`/Volumes/ExternalSSD/tmp/rev/c9-queda/libSDL2.so`, SHA-256
`239a88e53173df0febec24a7d38ba0759ad4e1e43c87c6e511e1d686d7d6f5d8`.
It lacks SDL_GetTicks64 and SDL_RWseek; it exports the surface constructor.
Do not infer API availability from its reported SDL version alone.

## Findings and backport

The inherited surface fallback (`fde68e4c1`) already exists in v2.0.8. The old
2.0.1/2.0.5/2.0.7 reports predate that fix; their surface-constructor failure
is not evidence that v2.0.8 needs that same patch again.

SDL_GetTicks64 appeared in the 2.1 code and fails on old SDL, including C9 and
webOS 3 (also upstream SDL 2.0.10/2.0.14 used by newer TVs). The coordinator's
`a877c0f7c` already replaces it. v2.0.8 has no SDL_GetTicks64 call.

v2.0.8 still calls SDL_RWseek in text.c. A small 2.0.x hotfix is worthwhile:
backport `e79037376` (one-line stream seek fix), plus this build guard to prevent
recurrence. The webOS 3.9.3 report count (~117 in ten days, supplied by the
coordinator) justifies prioritizing legacy compatibility, but does not measure
v2.0.8 failure frequency. Validate the hotfix binary and actual font rewind on
a TV before release. No old release branch was modified here.

There are no direct dynamic curl/OpenSSL imports in the inspected predecessor
binary. `rede.c` already opens curl .so.5 then .so.4 with RTLD_NOW, checks
required dlsym results, and treats reset/send/recv/share as optional where
appropriate. Its OpenSSL 1.0 locking callbacks are resolved dynamically and
used only when present. P2P OpenSSL is linked statically. Mixed libcrypto.so.3
and .so.1.1 crash frames across the fleet do not establish either as a webOS 3
requirement. No curl/OpenSSL replacement is warranted without an actual
forbidden import.

Source-wide SDL_GetHintBoolean (2.0.5) and SDL_OpenURL (2.0.14) calls are inside
Android and desktop/iOS branches respectively; they do not enter the webOS ELF.
The final binary audit, rather than a source grep, decides compatibility.

## Final ARM build (2026-10-30)

Built once with the requested local.properties and `--build --alto-cache`,
including default P2P, Rust, libass, DTS and DV7 paths. Exit status 0. The guard
passed immediately after linking: 85 SDL imports, highest GLIBC requirement
2.12 (below the 2.19 ceiling). `audited-sdl-imports.txt` records that exact SDL
set. Zero imports outside SDL 2.0.4; zero missing exports against the C9 copy,
W16N 3.4.0 and W17H 3.9.2 symbol dumps. No direct curl/OpenSSL imports and no
DT_NEEDED entries for curl, ssl or crypto. No additional runtime source change
was necessary on this branch, which already includes all three replacements.

GLIBC versions imported: 2.4, 2.6, 2.7, 2.8, 2.9, 2.11, 2.12. None exceed the
webOS 3 maximum. The portable test also proves rejection of SDL_GetTicks64,
SDL_RWseek, the newer surface constructor, GLIBC_2.20, GLIBC_2.100 (numeric
comparison), private glibc symbols, absent dynsym and readelf failure.

This checks the executable's import ABI, not every transitive vendor library,
SDL_image/SDL_ttf ABI, TLS handshake or playback behavior. No TV was deployed
to or exercised. The SDK linker warns that OpenSSL static archive makecontext
and related async helpers are unimplemented; this is separate from missing
dynamic imports and was not changed here.
