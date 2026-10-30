# webOS 2.0.8 network crash investigation

Release baseline: `v2.0.8`, `87c8b8909`. All 60 supplied records identify 2.0.8.
The input was parsed as data; no identities, timestamps, tokens or request
addresses are included here.

## Findings and confidence

The shared native network layer retained request options on successful cached
handles. `Vigia`, response header contexts and write contexts are stack objects;
POST data and error buffers also belong to the request or its thread. Those
pointers survived the request return until the next acquisition/reset. A handle
can instead be cleaned up by a pthread destructor or after connection warming,
parking and eviction on another thread. libcurl explicitly permits progress and
header callbacks during `curl_easy_cleanup`:
<https://curl.se/libcurl/c/curl_easy_cleanup.html>.

`soltarHandleDesde` now resets a successfully retained handle after collecting
all response information and before its request contexts expire. Reset preserves
connections, DNS and TLS sessions. Failed transfers are already cleaned up
synchronously while their contexts remain alive. This fixes a demonstrated
lifetime violation; it does **not** establish that it caused the supplied HTTPS
crashes. The documented cleanup callback examples involve other protocols.

Audit of the release and integration sources:

- All transfer paths set NOSIGNAL: common options, `rede_pedir`, and raw TLS.
  No missing option was found. The stress test verifies it before each transfer.
- Native webOS initialization runs before texture/application workers and is
  serialized with `abrirTrava` and acquire/release publication. Android intentionally
  initializes on its dedicated worker. Global init's result is not checked on the
  webOS path; no evidence links that omission to these crashes.
- CURLSH is Tizen-only and shares DNS with lock/unlock callbacks. Neither connection
  nor SSL-session caches are shared through it.
- Legacy cached handles have one thread owner. Warmed handles transfer ownership
  under `estacTrava`; no simultaneous handle use was found. No application SSL_CTX
  callbacks or cross-thread SSL_CTX storage were found. `redeurl.c` only parses
  addresses; image workers delegate downloads to `rede.c`.
- Legacy OpenSSL locking uses guessed sonames rather than curl's dependency
  scope. That deserves separate investigation, but modern OpenSSL 1.1/3 does not
  require those callbacks. Adding its sonames would not fix the supplied modern
  library signatures. No speculative locking change was made.
- `queda.c` handles fatal signals, not SIGALRM. ARM reports scan stack words rather
  than unwind frames. Repeated vdso entries can be stale candidates; they do not
  establish actual repeated signal delivery. Main's alarm is a shutdown watchdog.

## Symbolication

A detached checkout of the release was built successfully using
`NUVIO_EXTRA_CFLAGS=-g bash tools/arm.sh --build`, configured properties and the
`nuvio-webos-sdk` image through the project's Apple Container wrapper. Docker's
daemon was unavailable. Binary retained at
`/Volumes/ExternalSSD/tmp/queda-webos-208/nuvio-proto.arm`.

ELF is ET_EXEC; first load address is `0x10000`. `queda.c` subtracts the mapping
base, so **add `0x10000`** before addr2line, clearing the Thumb bit first.
Debug flags preserve optimization (`-O2`). No distributed-binary build ID was
provided to prove that its layout matches this rebuild; mappings are candidates.

| Report offset | ELF address | Rebuilt function | Source |
|---|---|---|---|
| `+0x80000` | `0x90000` | `desenhaFolhaFiltros` | `src/ajustes_ux_ilha.inc:2003` |
| `+0x393e0` | `0x493e0` | `buscarPedido` | `src/addons.c:3469` |
| `+0x8f6aa` | `0x9f6aa` | `ajc_neutro` | `src/ajcena.inc:113` |
| `+0x9475f` | `0xa475e` | `ajc_barra` | `src/ajcena.inc:127` |

Across 139 distinct application offsets, the most frequent resolved candidates
by report are `buscarPedido` (18), `nv_addon_base` (11), `ajcT_legendaLingua` (6),
`ajc_neutro` (5) and `ajustes_tamanho_ajustes` (5). Forty-two reports contain at
least one unresolved candidate. These are **not proven top network call sites**.
A stack scan includes integers and stale addresses; UI functions on a network
worker reinforce that limitation. Grouping the entire symbolicated application
candidate sequence gives no dominant caller stack: three reports have no
application candidates, two contain `nv_addon_base` twice, two contain
`estatvivo_retangulo`, and all remaining sequences occur once.

The claimed 44/60 signature does not appear as one exact PC/LR group:

| Count | PC | LR |
|---|---|---|
| 18 | libc + `0x61e88` | libcrypto.so.3 + `0x1099d3` |
| 12 | libc + `0x613cc` | libcrypto.so.3 + `0x1099c3` |
| 8 | libc + `0x5f6a8` | libcrypto.so.3 + `0x1179cf` |
| 6 | libpthread-2.28 + `0x9088` | libcrypto.so.1.1 + `0x11c64b` |
| 3 | libc + `0x61e88` | libcrypto.so.3 + `0x108e5f` |

Forty-four records have LR in libcrypto.so.3; another ten have LR in
libcrypto.so.1.1. Fifty-four report fault address `0x1c`. Six have SDL/Wayland or
unmapped non-crypto signatures. Exact library stack-prefix groups are 19 reports
for crypto `0x1099d3 / 0x12406f / 0x124473`, curl `0x4477b`; 10 for crypto
`0xd8f15 / 0xd8fd5 / 0xd9115 / 0xd92db`; 5 for crypto
`0x132617 / 0x132d67`, curl `0x4fd1f / 0xfd0f`.

Firmware library offsets cannot be reliably symbolicated with the SDK libraries:
identical sonames do not identify identical builds. A read-only check of the C9
found OpenSSL 1.0.2p/libcrypto.so.1.0.0 and curl 7.53.1/libcurl.so.5.4.0,
which do not match the modern report libraries. No app was installed or launched.

Reproduce the sanitized aggregate (only module/offset fields are emitted):

```sh
python3 tools/symbolicate-webos-crashes.py --reports /Volumes/ExternalSSD/tmp/rev/quedas-webos.json --binary /Volumes/ExternalSSD/tmp/queda-webos-208/nuvio-proto.arm --output /Volumes/ExternalSSD/tmp/queda-webos-build/symbolicated.json
```

## Regression and remaining proof

`bash tests/rede_lifetime.sh` failed on the original code (rc 134: retained handle
was not reset). It passes after the shared reset. The deterministic test checks
2,048 successful request lifetimes across 16 threads and cleanup after contexts
expire. The real local HTTP/TLS stress checks connection reuse, cancellation,
request deadlines and NOSIGNAL before each transfer. DNS timeout outcomes are
injected deterministically before real resolution; actual signal-driven resolver
races are not reproduced on the host.

The production root cause remains open. Needed: exact distributed ELF/build ID,
matching firmware library symbols or core dump from an affected modern TV, and
an on-device stress run showing whether this patch reduces the reported crashes.
Treat this patch as a narrowly proven lifetime correction, not a verified cure
for all 60 reports. It is suitable for a 2.0.x hotfix alongside continued diagnosis;
the minimal fix/test commit `d887c23e4` (`fix(network): detach request callbacks
before caching curl handles`) cherry-picked cleanly onto detached v2.0.8 as
`3538989e0`, and the full regression passed there. No hotfix branch was created.
`bash tests/webos_network_hotfix.sh` repeats that compatibility proof from an
archive without touching a worktree.

Final build results: release symbol build rc 0, local build rc 0, webOS high-cache
build rc 0 with configured properties, Android debug build rc 0 for arm64-v8a
and armeabi-v7a, including the 16 KB packaging gate. Android initially lacked
its default dependency cache; preserved artifacts compiled but failed page
alignment. The final build uses `/Volumes/ExternalSSD/cache-interno/nuvio-android`,
whose curl/jpeg/webp libraries pass that gate. TMPDIR and ccache are on the SSD.
No push, installation or deployment was performed.

`bash tests/webos_network_builds.sh` runs all requested final builds (optional
arguments select targets). `bash tests/webos_network_build_results.sh` checks
recorded exit codes and artifacts without repeating full builds; this is a
receipt check, not an independent rebuild.
