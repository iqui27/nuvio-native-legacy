# #736 integration — TPK40 experimental DTS (default OFF)

Status: source integrated into `integracao/2.1`. Experimental only. Default OFF. TV behavior UNKNOWN. Issue #736 stays open.

## Identity
- Integration branch: `integracao/2.1`, worktree `/Volumes/ExternalSSD/nv-21-int` (sole writer).
- BASE: `4719643de2750b75a110523f3624c37a15683ce7`
- Candidate (reviewed head): `48e105cecf8db3f88581f9a9ec96606f8a9faa0e` (`agente/21-21-fv-av736-optin`)
- Actual merge: `6c035dd5e028fcbdd9949a3945bf0e64fdc1c12f`. Parents asserted exactly: `6c035dd5e` → `4719643de`, `48e105cec`. MATCH.
- Candidate diff (`git diff BASE HEAD`): sha256 `1d0a89c62719f0f56049f89fd373706d837e205ae03b18a09c600b315725d6b9`. The brief labelled this SHA1; the value is SHA256. Verified by hashing the diff with `shasum -a 256`. SHA1 of the same diff is `2cbdd82bcb27e4d03f279e2d01f3b1ae3ed8b13d`, not used as evidence.
- Canonical evidence digest: `9f4f3aed35f944ec4068666f81f17b315589bbaa90a1bde79ba48fb1bf5fbdf3`.
- Files changed (6, +207/-30): `src/dts/dts_hls_tpk.c`, `tools/tpk.sh`, `tests/dts_tpk_legacy_gate.c`, `tests/dts_tpk_legacy_gate.sh`, `tests/tpk40_experimental_output.sh`, `tests/tpk40_texto_nome.sh`.

## Reviewers
- Static reviewer: `fv-av736-static-review`. Model: UNKNOWN.
- Runtime verifier: `fv-av736-runtime-verifier`. Model: UNKNOWN.
- Verifier results: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/av736-verification.results.json`.
- Canonical record: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/av736-canonical-evidence.json`.
- Canonical gate: `passed`. Declared 5, executed 5, required 5, passed 5. Skipped 0, failed 0, timeouts 0, divergences 0, errors empty. Agreement true.
- Canonical 5 checks, actual rc from the record:
  1. `bash tests/dts_tpk_legacy_gate.sh` rc 0
  2. `bash tests/tpk40_texto_nome.sh` rc 0
  3. `bash tests/tpk40_experimental_output.sh` rc 0
  4. `bash tools/build-local.sh` rc 0 (1 warning)
  5. `python3 tools/varredura-i18n.py` rc 0. stdout sha256 `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1` (as supplied by central)
- Process note: the verifier's first i18n attempt shared a tool call with build-local. The harness 60 s limit killed it with no rc. Rerun alone: rc 0. Not a script failure.

## Post-merge re-run (at merged HEAD `6c035dd5e`)
Environment: `NUVIO_PROPERTIES=/dev/null TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp/fv-av736-postmerge`. Git status empty before tests.
- `bash tests/dts_tpk_legacy_gate.sh`: rc 0. Log `av736-postmerge-dts-gate.log`. stdout sha256 `0a4e4661ecdbd44282471881a3cad5a4cfe7a7223f1c33433b33f99af44ae2c8`. Matches canonical check 1.
- `bash tests/tpk40_experimental_output.sh`: rc 0. Log `av736-postmerge-output.log`. stdout sha256 `a430f0efec84c0e2afbcd277e0fce09eb404722cf7dbefe4d5ac984a6dd9501b`. Matches canonical check 3. Test stubs `tpk_sdk` (rc 42) before any container call, uses `HOME` and fixtures under `TMPDIR`.
- No SDK build was run in this post-merge step.

## SDK proof (from verifier results, offline, cached)
Environment: Apple container CLI 1.5.0; cached image `nuvio-tpk-sdk-arm64`, unchanged; gcc 8.3.0; ld 2.31.1; dotnet 10.0.402. Cache `~/.cache/nuvio-tpk`, carimbo equal to sha of `tools/build-dts-ffmpeg.sh` at BASE and HEAD, so no network branch. NuGet forced offline. Signing: bundled public `author_test.p12`. No user keys, no publish, no install.

Four builds, all rc 0:
| build | rc | log sha256 (prefix) |
|---|---|---|
| BASE default | 0 | 4002eba6… |
| HEAD default | 0 | 3eda75c7… |
| HEAD experimental (`NV_TPK40_DTS_EXPERIMENTAL=1`) | 0 | 3fb4c5f9… |
| HEAD default after experimental | 0 | 5b70753c… |

Artifacts (full sha256 in results file):
- `libnuvio.so` BASE = HEAD: `465263925bc06f4599028bb7316b90784b7aadefcabdc0dbf5ddf58056e7f209` (byte-equal).
- `libnuvio-tpk40.so` BASE `28c9c8c6bc59145283d434942c8518701f228bc0792f7eca9d6c6d1369eca863`; HEAD `a0abb5d5e02de8f336dd02f5aba5688eee9a785ac335b66a3f345ae0d01f7905` (equal to author-retained artifact).
- Experimental `.so` `cad020bd192d5c6d85010e9b23296260c8c70079a7870995f4ad0456ccc65a40`; experimental `.tpk` `d7c97216aabd6bf40bead756515717274237b446c2a5442fc7a865650a78f1de`.

### Default BASE vs HEAD equivalence (ELF)
- Program headers equal.
- All 23 SHF_ALLOC sections equal in size, align, addr, flags, sha256, excluding `.note.gnu.build-id` content (the only alloc difference). `.bss` equal size and align.
- Symbols: 69857 = 69857. 7 local STT_OBJECT entries differ only in `.N` ordinal (+2 each), all under STT_FILE `dts_hls_tpk.c`. Value, size, binding, section equal. `.strtab` +8 B = one new unshared string `b.25203`. Static reviewer explanation confirmed at artifact level.
- `.tpk` (1422 entries): differences limited to `lib/libnuvio.so`, signatures, and `bin/NuvioTpk40.dll/.pdb` (embedded pdb path, src-base vs src-head).

### Experimental (HEAD) checks
- PT_TLS 0, R_ARM_TLS 0, `__tls_get_addr` 0, DT_HASH present.
- Relocs: GLOB_DAT 21, JUMP_SLOT 391, RELATIVE 327881; subset of what Program40 accepts.
- Max glibc GLIBC_2.28. 22/22 DllImport exports present.
- FFmpeg static: no libav NEEDED; 651 av_/avcodec_/swr_ symbols defined.
- LGPL COPYING and SOURCE.txt in package, equal to cache.
- Experimental manifest sha equals default (`f45548a8…`): same package id and version (see R-b).
- Import delta vs default: 17 new undefined symbols. One needs glibc ≥ 2.28: `fcntl64@GLIBC_2.28` from `libavutil.a:file_open.o` (F2).

### Default after experimental
- rc 0. `.so` identical to first HEAD default. All 1422 tpk entry CRCs identical. 0 `licencas/dts` entries in Tpk40.
- The shared dotnet obj did not contaminate default output.

### Default-path equivalence and writes
- Default output after experimental: identical (see above). Fail-safe proven at artifact level.
- Experimental writes: new files under `build/tpk/experimental-dts`; also writes `tizen-tpk/NuvioTpk40/{lib,res,obj}` (same paths default uses). MSBuild IncrementalClean deleted 1421 files in `tizen-tpk/NuvioTpk40/bin/Release/tizen40/tpkroot` (N1, see below). Top-level default outputs and update attachments unchanged.

## Mutation check (scratch only)
- SEM_DTS guard deleted → `dts` test still rc 0 (R-d false pass reproduced). In a real build the in-container `Experimental TPK40 requires FFmpeg` check still fails closed.
- TEXTO guard deleted → test rc 1 (caught).

## Findings (from verification results; no P0/P1)
- F1 (P1 texto naming): WITHDRAWN. Real default names equal BASE; guard rejects `TEXTO=1`.
- R-a (P2 cache miss): WITHDRAWN. KEY already hashes `tools/tpk.sh`.
- R-b (P2): CONFIRMED. Experimental manifest equals default (same id/version), so next release `-tpk40-arm.so` auto-update would replace the DTS lib. Device-test validity issue only.
- R-c (P3, static): stale comments at `tools/tpk.sh:306` and `:349`. Kept as note.
- R-d (P3, mutation): see above. Kept as note.
- R-e (P3, static): `tests/dts_tpk_legacy_gate.sh:11` enumerates only versions 3..6; unknown version not covered. Kept as note.
- R-f (P2): INCONCLUSIVE. FFmpeg memory/CPU/ABI on Tizen 4/5. Host build cannot prove TV load or playback.
- F2 (P2): INCONCLUSIVE, part of R-f. Experimental lib imports `fcntl64`, which exists only in glibc ≥ 2.28. Program40 ELF loader resolves externals by unversioned `dlsym` and refuses unresolved symbols. On a Tizen 4/5 TV with glibc < 2.28 where memfd dlopen fails, experimental lib could be refused; default path has no `fcntl64` and is unaffected. Needs device run.
- N1 (P3, fail-safe): experimental build's IncrementalClean deletes default ephemeral `bin/Release tpkroot` and rewrites shared obj `FileListAbsolute`. Default outputs after it are byte/CRC identical. Shared staging cleanup, default artifacts restored same.

## Not claimed
- Not a TV acceptance. Physical Tizen 4 and 5.5 runs: PENDING, separately authorized.
- No field fix claim. Issue #736 stays `aberta`.
- No release graduation. Default stays OFF. No `-tpk40-arm.so` change.
- No BIB639/PAG789 global gate waiver. No whole-campaign completion claim.
- No device, install, publish, push, signing keys, or network use in post-merge step.

## Docs gap
- `.claude/central.md` exists at canonical path `/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy/.claude/central.md`. The verifier reported it missing from the worktree; the worktree has no such file. Not a blocker. Central file read: integration worktree and mapa validation rules.
