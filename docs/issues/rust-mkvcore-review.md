# mkvcore Rust best-practices review

Branch: `agente/205-rustbp`, based on `integracao/2.0.5`. Scope: the sole crate `rust/mkvcore`, all six C exports, their `src/mkv.c` callers, and build profiles. References: rust-best-practices chapters 01–09 in `/Users/hrocha/.agents/skills/rust-best-practices/references/` (the catalog's `.pi` path is missing). The harness has no Skill tool; the skill and chapters were invoked by reading their files. Ponytail full: preserve ABI, use core, add no dependencies or speculative abstractions.

Findings: **P0: 0; P1: 1; P2: 3. All fixed.** Locations below refer to the reviewed/fixed source.

| Priority | Location | Scenario and resolution |
| --- | --- | --- |
| P1 | `rust/mkvcore/src/lib.rs:586` | `diag_trecho` called `quadro_rpu` without the exported frame parser's 1..=4 NAL-length guard. A five-byte length on a complete SimpleBlock falsely reported no RPU (0), whereas C rejected the frame and retained unknown (-1). Validate at the shared parser, preserving the outer diagnostic scan, including truncated-frame offsets. The internal helper also overflowed `o + nal_tam + 2` for `i64::MAX` in debug tests; that extreme is not directly reachable through the `c_int` ABI. |
| P2 | `rust/mkvcore/src/lib.rs:164` | `buf` checked NULL/nonpositive length but relied entirely on the caller for slice range arithmetic. Reject address wrap and lengths above `isize::MAX` before `from_raw_parts`. Valid buffers behave identically. This cannot validate arbitrary allocation provenance: readable allocation/lifetime remain caller obligations. |
| P2 | `rust/mkvcore/src/lib.rs:798` (also 812, 846, 889, 914, 935) | Safety docs only said “valid/writable”, omitting alignment, immutable input lifetime, one allocation, output capacity and nonaliasing. State these obligations on every export and the slice helper. Every unsafe block already had a `// SAFETY:` comment; preserve them and enforce their presence. Document the link-only personality symbol. |
| P2 | `rust/mkvcore/Cargo.toml:14`, `tests/rust_mkvcore.sh:1` | Only four inline Clippy lints were enforced and no standalone Rust check runner existed. Centralize package lints, deny `all`, unchecked indexing, unwrap/expect/panic, undocumented unsafe, redundant clones/collects, missing docs and implicit unsafe operations. Replace the remaining fixed-array indexing/slicing with checked access/as_slice. Add a runner using the product SSD toolchain; keep test-only abort declarations gated out. |

## Reviewed without further changes

- Borrowing/allocations (chapters 01, 03, 09): inputs and subelements borrow `&[u8]`. No clone, heap allocation, allocator, Vec, String, Box, Arc or intermediate collect exists. Temporary ABI records and the eight-byte RBSP prefix stay on the stack; recursion is limited to eight levels.
- Iterators/dispatch (01, 06): `fold`, `zip`, `take`, `any` and generic `FnMut` already serve transformations with static dispatch. Offset-driven EBML loops need early exits and variable strides; an iterator rewrite would add scope without evidence of benefit.
- Errors (04): `Option` plus `?` describes an invalid/absent header; C-compatible negative sentinels distinguish malformed/unknown NAL or EBML data. `unwrap_or` supplies nonpanicking defaults. No runtime unwrap, expect, panic, todo or unreachable macro exists outside tests. ABI layout assertions are compile-time constants. A new Result hierarchy/thiserror dependency would not improve the caller's observable error contract.
- FFI/panics (02, 04, 08, 09): `no_std`, dev/release `panic="abort"`, the aborting panic handler, and `rust_eh_personality` are retained together. No unwinding reaches C. `catch_unwind` is neither available from core nor effective for aborting builds. Arithmetic in normal parsing is bounded by readable slice size, small EBML widths, checked/saturating additions or explicit wrapping counters. No caller-reachable panic was reproduced on valid FFI contracts.
- Pointers (09): NULL optional outputs and nonpositive capacities cause no writes; output pointer arithmetic stays below supplied capacities. Writable alignment, allocation provenance and disjointness cannot be inferred from a C pointer; documented unsafe preconditions are required. `repr(C)` layouts retain existing compile-time size assertions. No global mutable Rust state.
- Type state (07): no lifecycle requiring new states; keep the small parsing functions.

## Regression evidence

`diag_should_return_error_when_nal_length_invalid`: before the shared guard, `bash tests/rust_mkvcore.sh` failed with 0 instead of -1 (rc 101); after, passes. `quadro_should_return_error_when_nal_length_invalid` starts with `i64::MAX`: temporarily removing the guard reproduced `attempt to add with overflow` (rc 101); restoring it passes. Tests also cover all truncated prefixes of an EBML header and a length-prefixed frame, and rejection of a wrapping pointer range. Five unit tests, no new test dependencies.

Integration: `bash tests/mkvcore_paridade.sh` passed **25,512** guarded C/Rust field comparisons, including truncation, mutation, random bytes and large declared sizes. `NV_RUST_MKVCORE=1` runs of `tests/mkv_contentor.sh`, `tests/mkv_caps.sh`, `tests/mkv_dvrpu.sh` and `tests/mkv_legendas.sh` passed. The latter exercises ASS/SRT extraction and switching tracks against the Rust-backed MKV parser. No need to run the C-only `mkvass.sh` as redundant coverage.

Product checks completed with rc 0:

- `bash tests/rust_mkvcore.sh`: locked Clippy all-targets with `-D warnings`, plus five Rust tests.
- `NUVIO_RUST_MKVCORE=1 NUVIO_MAC_SO_COMPILAR=1 NUVIO_MAC_BIN=/tmp/nuvio-205-rustbp-mac bash tools/mac.sh`: product Mac link with Rust ON, compile only.
- `bash tools/arm.sh --alto-cache --build`: high-cache ARM product build only. This branch's webOS ARM script uses the C core (Rust is enabled in Mac/Android product builds); it is a build compatibility check, not an ARM Rust execution test.

The product scripts warned about missing local server configuration; compilation and linking still returned 0. No push, deployment or device installation was performed.
