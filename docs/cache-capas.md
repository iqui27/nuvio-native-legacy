# Artwork cache standard

Owner: “temos que ter um padrao para o cache e capas, é o mais importante; se quiser veja outros ou como nuvio ou forks fazem; precisamos resolver para nao piorar toda vez”. Research retrieved with `curl --max-time 20`, read-only, 2026-10-22. No third-party code copied. MiB means 1,048,576 bytes; RAM thresholds use the app's reported MemTotal MB.

## Findings and sources

- **Official Nuvio TV**, revision `ddaadaf03209be23167dfdca42ede39c52e75f3a`: [NuvioApplication.kt](https://github.com/NuvioMedia/NuvioTV/blob/ddaadaf03209be23167dfdca42ede39c52e75f3a/app/src/main/java/com/nuvio/tv/NuvioApplication.kt#L145) selects Coil memory cache at 15% of Android heap on <=2 GiB devices, 20% <=3 GiB, 25% above; these percentages are **heap**, not total physical RAM. Disk 200 MiB, hardware bitmaps off, inexact decode, four BitmapFactory jobs. [ContentCard.kt](https://github.com/NuvioMedia/NuvioTV/blob/ddaadaf03209be23167dfdca42ede39c52e75f3a/app/src/main/java/com/nuvio/tv/ui/components/ContentCard.kt#L260) sets requested width/height and includes size/revalidation version in memory keys. [NetworkModule.kt](https://github.com/NuvioMedia/NuvioTV/blob/ddaadaf03209be23167dfdca42ede39c52e75f3a/app/src/main/java/com/nuvio/tv/core/di/NetworkModule.kt#L109) has a separate 50 MiB HTTP cache; do not confuse it with artwork disk storage.
- **Official Nuvio Smart**, revision `a4e302f265227b873197423c0bc3b9a5acf7caca`: [homeImageCacheStore.js](https://github.com/NuvioMedia/NuvioTVSmart/blob/a4e302f265227b873197423c0bc3b9a5acf7caca/js/data/local/homeImageCacheStore.js) remembers up to 500 URL records for 30 days. URL metadata is not a 500-bitmap residency guarantee. Browser/OS owns decoded/GPU caching. [Nuvio Mobile platform loader](https://github.com/NuvioMedia/NuvioMobile/blob/cmp-rewrite/composeApp/src/androidMain/kotlin/com/nuvio/app/core/ui/PlatformImageLoader.android.kt) was also checked; TV's explicit policy is the useful reference.
- **Tuvora TV**, local research `/Volumes/ExternalSSD/tmp/tuvora-estudo/RELATORIO.md`, checkout `cda27aa8ac82bbcc64926c0501cb5796f4013333`: report identifies Coil AsyncImage at fixed 30 dp for guide logos. Additional inspection of `core/memory/MemoryTierPolicy.kt` and `NuvioApplication.kt:330` finds 32/64/96 MiB image memory tiers based on Android low-RAM flag and heap memoryClass (<=192/<=320/>320), 200 MiB disk, two decodes, hardware bitmaps and RGB565 allowed. Comments cite swap thrash with ~127 MiB largeHeap cache on 2 GiB boxes. Independent policy observation only; GPL implementation not copied. **Nuvio Reshaped:** GitHub public repository search `nuvio reshaped` returned no results; no verified cache policy.
- **Stremio:** public [web Image component](https://github.com/Stremio/stremio-web/blob/development/src/components/Image/Image.tsx) uses browser `<img loading='lazy'>`, error fallback and URL identity. Public [shell-ng](https://github.com/Stremio/stremio-shell-ng) tree exposes no Android TV poster-cache policy. `Stremio/stremio-android-tv` and `Stremio/stremio-tv` returned 404. Do not claim a TV MiB budget or derive poster residency from streaming-video cache settings.
- **Jellyfin Android TV**, revision `56c6fc44dc4bd1898ab25af030df575cd25f4618`: [AppModule.kt](https://github.com/jellyfin/jellyfin-androidtv/blob/56c6fc44dc4bd1898ab25af030df575cd25f4618/app/src/main/java/org/jellyfin/androidtv/di/AppModule.kt#L112) uses singleton Coil ImageLoader with OkHttp/GIF/SVG decoders; inspected builder does not override memory/disk budgets, so Coil defaults apply there. **Plex:** [cache support article](https://support.plex.tv/articles/202967376-clearing-plugin-channel-agent-http-caches/) concerns server/plugin HTTP caches, not TV posters. No public Android TV client cache implementation verified; numeric TV budget unknown.
- **Glide:** [MemorySizeCalculator](https://github.com/bumptech/glide/blob/master/library/src/main/java/com/bumptech/glide/load/engine/cache/MemorySizeCalculator.java) targets two ARGB screenfuls in memory cache; bitmap pool targets four screens before Android O, one on O+, zero on O+ low-RAM. Combined memory/bitmap/array pools capped at 40% of memoryClass (33% low RAM), array pool normally 4 MiB, halved on low RAM. These are caps, not a fixed 40% bitmap cache. [DiskCache](https://github.com/bumptech/glide/blob/master/library/src/main/java/com/bumptech/glide/load/engine/cache/DiskCache.java) defaults 250 MiB. [Targets](https://bumptech.github.io/glide/doc/targets.html) resolve display size for downsampling; [RecyclerView preloader](https://bumptech.github.io/glide/int/recyclerview.html) requires matching sizes/options and an explicit preload count. LRU resources and reuse reduce repeated allocation; preloading is not permission to fill the heap.
- **Coil 3:** [Android contexts](https://github.com/coil-kt/coil/blob/main/coil-core/src/androidMain/kotlin/coil3/util/contexts.kt) defaults to 20% heap, 15% low RAM. [MemoryCache](https://github.com/coil-kt/coil/blob/main/coil-core/src/commonMain/kotlin/coil3/memory/MemoryCache.kt) uses bounded LRU strong references plus optional weak references. [DiskCache](https://github.com/coil-kt/coil/blob/main/coil-core/src/commonMain/kotlin/coil3/disk/DiskCache.kt) defaults to 2% free disk, clamped 10–250 MiB, LRU. [Coil 2 migration](https://coil-kt.github.io/coil/upgrading_to_coil2/#remove-bitmap-pooling) explicitly removed bitmap pooling. [Image loaders](https://coil-kt.github.io/coil/image_loaders/) recommends one shared loader; size-aware requests and bounded explicit enqueue prefetch avoid duplicate caches. Coil defaults are version-specific; no universal automatic row-count preload.

Our constraints differ: native GLES textures share physical RAM with decoder surfaces and video; webOS 650 MB is not an Android managed heap. Adopt bounded tiers and target-size decode, not another app's percentages verbatim.

## Standard: ownership and budgets

`src/perfiltv.c` is the single executable budget source (`ptv_tex_auto_mb`, `ptv_tex_teto_mb`, `ptv_padrao`, `ptv_cache`). No duplicate per-platform budget in the cache. Existing user-selected texture ceilings remain bounded by that source. Android values reuse parallel job `e23561f55`; do not reintroduce 64 MiB TCL default.

<!-- perfiltv-sha256: a5a280b25252ca9fa1ed6e8be06b12b8db10ce4cb611567a2e56ede86de5155e -->

One table, all platforms and RAM classes. `GPU auto/max`, `CPU`, `disk`, `body` are MiB; hero width is pixels. CPU includes active reservations and completed decoded surfaces, not a second long-lived image cache. Each active decode reserves `reservaDecodeMb` (48 MiB, table column); at most two workers, admission also obeys CPU byte cap. Upload allowance is **5 ms / 1 MiB strip** on all classes; compressed body cap **8 MiB/request**. Network columns show active worker ceilings.

| Platform | RAM MB | GPU auto/max | CPU cap | Decode res. | N rows each side | Hero default | Network | Disk maximum |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| webOS | 650 | 48/64 | 64 | 48 | 1 | 1280 | 2 | 512 |
| webOS | 1500 | 128/160 | 96 | 48 | 2 | 1920 | 4 | 512 |
| webOS | 2400 | 128/300 | 96 | 48 | 2 | 1920 | 4 | 512 |
| webOS | 4096 | 192/512 | 96 | 48 | 2 | 1920 | 4 | 512 |
| Tizen wgt | 650 | 64/64 | 64 | 48 | 1 | 1280 | 2 | 48 |
| Tizen wgt | 1500 | 96/96 | 96 | 48 | 2 | 1280 | 2 | 48 |
| Tizen wgt | 2400 | 96/96 | 96 | 48 | 2 | 1280 | 2 | 48 |
| Tizen wgt | 4096 | 128/128 | 96 | 48 | 2 | 1280 | 2 | 48 |
| Tizen tpk | 650 | 64/96 | 64 | 48 | 1 | 1280 | 2 | 512 |
| Tizen tpk | 1500 | 96/160 | 96 | 48 | 2 | 1280 | 4 | 512 |
| Tizen tpk | 2400 | 96/160 | 96 | 48 | 2 | 1920 | 4 | 512 |
| Tizen tpk | 4096 | 128/256 | 96 | 48 | 2 | 1920 | 4 | 512 |
| Android TV | 650 | 32/32 | 64 | 48 | 1 | 1280 | 2 | 64 |
| Android TV | 1500 | 96/96 | 64 | 48 | 2 | 1920 | 4 | 64 |
| Android TV | 2400 | 160/160 | 80 | 48 | 2 | 1920 | 4 | 64 |
| Android TV | 4096 | 256/256 | 96 | 48 | 2 | 1920 | 4 | 64 |

Thresholds between examples remain authoritative in code: webOS <1200/<3000, wgt <=1024/<4096, tpk <1200/<2500; Android <1500/<2000/<3000/<4000. Unknown RAM uses conservative existing GPU defaults and N=1/CPU=64. Tpk weak GPU limits hero to 1280; Utgard to 960. Wgt can select 1920 at >=2000 MB with High quality. Low-RAM LG values retain measured OOM headroom; TCL 160 MiB restores rows without relaxing trim. Native disk maximum is reduced to 15% of free+cached storage, minimum 32 MiB; below 300 MiB free use 32 MiB, preserve 128 MiB free-space reserve. Disk is compressed data, not resident RAM; GPU+CPU is not total process RSS.

Tiers:

1. **GPU:** ready RGBA textures plus CPU-generated mip-chain accounting. Authoritative texture budget covers old+new texture during promotion. No shared upload worker/context.
2. **Decoded RAM:** temporary surfaces awaiting upload and codec reservations only; released after upload. No duplicate retained decoded poster cache or bitmap pool. Keeping nearby GPU entries avoids both network and disk re-decode.
3. **Compressed disk:** URL/variant-keyed original compressed response, atomic temporary+rename, asynchronous bounded writer (32 entries, 24 MiB), disk-index LRU; never decoded RGBA. On wgt persistent IndexedDB is separate from fixed WASM heap. Lookup may use HTTP cache if persistence unavailable.
4. **Network:** miss only. Bounded 8 MiB bodies; queued compressed responses <=GPU budget/4 before publication, plus at most one bounded response per active worker waiting outside queue. Writer copies have their own 24 MiB cap. Negative failures back off rather than fetch per frame.

Keys: full logical URL identity in texture cache, actual requested width variant in disk URL via `arte_tamanho_url`; native file name derives existing URL hash. Size promotion checks actual decoded width/source width. Changed provider URL/query yields different identity; never strip credentials/query to merge unrelated artwork. Hash filenames are lookup identifiers, not cryptographic collision proofs. Preserve existing identity/variant format to reuse installed caches.

## Residency and eviction

Visible art (current and previous rendered frame), current hero/crossfade and current detail are never evicted, including forced eviction. Home declares cached horizontal viewport slices for every row before admitting new requests; duplicate URLs keep minimum distance. Retain focus row plus N above/below. Registration never fetches or decodes. Offscreen horizontal columns are not pinned; this prevents an entire long catalog from becoming unevictable. Other screens retain their visible set through normal draw requests; row-distance residency currently belongs to Home.

When full, defer new requests/uploads instead of evicting protected artwork or exceeding budget. Outside protected set, discard surplus transient GIF/old hero frames first (retain at most three cold transient frames), then farther rows before nearer rows, then LRU on equal distance; unclassified old artwork has greatest distance. Normal residency promise applies to already loaded same-size art while inside the moving window. Leaving N and returning later can require disk decode; no claim that all 20 rows fit simultaneously. New rows, changed URLs, size promotion and cold startup are initial loads.

On trim/low memory, cancel speculation and release cold CPU work; remove neighbourhood protection, shed farther rows first while retaining visible set. Timed Android trim recovers gradually; sampled MemAvailable uses hysteresis; playback lowers cold-art residency. Window zero-miss promise is suspended under explicit trim, never visible-texture protection. If even visible art exceeds a user-reduced ceiling, retain existing visible art and queue uploads; do not manufacture budget compliance by deleting pixels on screen.

## Decode, upload and prefetch

Decode to target width (display width rounded to existing cache bucket; card ceiling 640, lite 320; hero profile as table). JPEG uses DCT subsampling, WebP scaled decoder, wgt browser worker downsampling; Android uses BitmapFactory bounds/inSampleSize, output <=2 Mi pixels, first GIF frame on low-RAM devices. Prefer native scaled WebP before SDL fallback. Two decode workers plus byte reservation cap apply on all platforms, so small devices admit only one 48 MiB reservation at a time; completed surfaces remain charged. CPU mips are part of surface accounting.

**Remaining codec limitation:** native webOS/tpk PNG and missing native JPEG/WebP library fallback still use SDL_image full-image decode before reduction. Reservations bound concurrency, not the codec's internal allocation. Strict downsample-at-decode for those fallbacks is not yet implemented; do not certify arbitrary 4K PNG memory peaks from this host contract. Android has no full-size SDL fallback.

Only renderer thread and its original current GL context may call upload pump; mismatched thread/context returns without GL work. Upload pump uses try-lock, at most three small uploads per class and 5 ms measured work per call, 1 MiB strips for larger surfaces, one expensive residency step per frame. Allocation itself can exceed allowance on a stalled driver: 5 ms is admission scheduling, not a hard preemption guarantee. Track `tex_upl_bytes`, `tex_upl_n`, upload timing and >16 ms trace; no glFinish/shared-context publication.

Prefetch: at rest after two stable frames, at most one new request/frame in row neighbourhood; during movement at most one/two frames on entering edge. Requires free slot, <75% GPU budget, no trim, bounded outstanding speculative entries; never evict to speculate or promote resolution for prefetch. Resident row registration is independent of speculative admission.

## Contract and acceptance

`bash tests/cache_contrato.sh` is discovered by `tools/testa-tudo.sh` and uses real `tex_cache.c`, profile table and host GL. It walks down 20 rows and back for every table example, six posters/row, forces cache-slot churn, checks overlapping-window misses (network/decode would be required), stable returned texture IDs, GPU-byte admission, no hot eviction and wrong-thread upload refusal. Network/codec boundary uses generated decoded fixtures: this is deterministic cache-policy proof, not an HTTP/JNI/TV benchmark. Existing tex_memory/texdisco tests cover scaled JPEG allocation and actual compressed-disk behavior.

Good on target: after warming, zero network refetch and zero disk re-decode for unchanged rows continuously within N of focus; no frame >250 ms; resident GPU and decoded bytes under their independent budgets, total process RSS/PSS leaves video/OS headroom. Measure cold/warm down20/up20, hero/detail transitions, GIF rows and trim on 650 MB LG, 1.5 GB, 2.4 GB TCL and 4 GB. Host timing cannot prove TV frame deadlines or driver memory. No device deployment performed by this task.

`python3 tools/cache-contrato.py` checks SHA256 of `perfiltv.c` against this document and committed history since introduction. Every later source revision requires updated marker/document in the **same commit**. Hash covers the complete source to catch indirect policy changes; optimizer-only edits also require marker update. Update prose/table intentionally before refreshing marker; marker is coupling, not proof of prose truth.

Validation on this branch: contract first failed with six resident-window misses on 650 MB LG (`rc=1`), then passed all 16 platform/RAM examples (`rc=0`). `tests/cache_regressoes.sh` runs existing texquente, texpressao, texb3, tex_nonblocking, texdisco, texbruto, texfila, teximgloop and tex_memory with shared host objects; final rc=0. Local build and Android debug build (arm64-v8a/armeabi-v7a, configured properties file without printing values) returned rc=0. Updated fixtures establish real startup budget/running state and expect gradual trim recovery and eight 1 MiB strips. Contract compiler runs independent objects in parallel to fit the suite timeout. Earlier interrupted runs are not acceptance evidence.

Regression history: `44014cf78` introduced shared GL upload plus glFinish; `a60aa5f86` removed it. `211efce22` added Android subsampling/backpressure but reduced TCL GPU cache to 64 MiB; parallel `e23561f55` restores 160 MiB. See [TCL memory investigation](reviews/tcl-trava-memoria.md): 25,961 ms 4K decode, 2,504.6 ms GL clear stall, 144 MiB GPU artwork. Old upload and GIF-row regressions justify resident-set, allocation and main-context gates independently of increasing a ceiling.
