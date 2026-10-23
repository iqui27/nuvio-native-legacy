# Live TV PR triage

**Decision: accept 587/597; port the complete 596/601/602 feature set onto livefiltro. Block blind merging of the guide stack. No release feature may be dropped.**

Offline review. Base `integracao/2.0.5` pinned to `2e38d5397f20ad526b2163d9ba2689891915b9a0`; livefiltro committed source pinned to `e35ee7ac6e35cde820f9d6c3e941c8a42cb0c43a`. Livefiltro was dirty and changed during review: its owner started editing `guia.c`, `stalker.c/.h` and regression fixtures. Those unfinished changes are not approved here. Downloaded descriptions are claims, not independently reproduced device evidence.

## PRs, dependencies and applicability

Sizes include tests/translations. Direct check = `git apply --check`; three-way = `git apply --3way`, each against the pinned base in disposable repositories under `/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage-v2`. No patch applied to either shared worktree.

| PR | Behavior | Files; additions/deletions | Direct / three-way rc | Dependencies; verdict |
|---|---|---:|---|---|
| 587 | VOD-only source filters; custom/live resources bypass them; persisted per-add-on override; mixed lists, reload and failover retain scope; live-only sheet hides VOD chips. | 57; +348/−51 | 0 / 0 | Independent. **VEREDITO: ACEITAR** |
| 597 | Signed Android progress samples; cancel recovered stalls; bounded same-player HLS window/reset recovery; stale-manifest revalidation; buffer/allocator diagnostics. Shared native watchdog also changes. | 16; +1286/−56 | 0 / 0 | Independent. **VEREDITO: ACEITAR** |
| 596 | Named, ordered, enabled per-profile Xtream/Stalker/M3U accounts; legacy migration; account/source picker and category order; M3U metadata/headers/XMLTV; isolated IDs/cache/EPG; load states, refresh, low-memory EPG and sign-out cleanup. | 108; +4271/−325 | 1 / 1; 35 conflicts | Parent of 601/602. Preserve all features through port. **VEREDITO: PORTAR** |
| 601 | Category draft; Confirm saves once; Cancel/Back discard; failed save keeps draft; checkbox/footer/account totals; large-guide tests. | 112; +4530/−332 cumulative. Increment: 8 files, +293/−41. | 1 / 1; 35 conflicts | Includes 596. No new guide storage/admission implementation. **VEREDITO: PORTAR** |
| 602 | Repeated-search modal/keyboard ownership; incremental EPG search, copied titles, stable sorted publication; revision restart and close cancellation. | 118; +4844/−389 cumulative. Increment: 7 files, +316/−59. | 1 / 1; 35 conflicts | Includes 596+601. **VEREDITO: PORTAR** |

All three guide patches conflict in the same set: `ajustes.c`, `ajustes_ux_dados.inc`, `ajustes_ux_padrao.inc`, `epg.c`, `guia.c`, `guia.h`, `xtream.c`, and 28 translation headers. Full lists: `/Volumes/ExternalSSD/tmp/rev/livepr/pr{596,601,602}.conflicts.txt`; application logs: `/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage-v2/apply-*.log`. Additional direct-check failures merge automatically in three-way; do not equate direct failures with unresolved conflicts. 587+597 also pass sequential direct application together.

These downloads are cumulative diffs against upstream. Integrate 596 once, then only the 596→601 and 601→602 increments; never apply all three complete downloads sequentially. Exact touched-file PR trees were reconstructed from local old blob IDs plus downloaded patches. Citations below refer to those PR trees, not current integration line numbers.

## P0–P2 findings and limits

- **596, inherited by 601/602 — P2: Stalker selection happens after truncation.** [PR596 src/guia.c:1209](/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage/pr596/src/guia.c:1209) sets `cota = sTeto - sRetidos`, clamps to 1,200, fetches only that prefix at line 1218, then checks selected groups at line 1225. Example: 900 available TV slots, first 900 channels outside selected group, wanted channels later. Empty result despite unused slots; even worse than fetching the old 1,200 prefix. `stalker.c:551` stops at caller capacity. Port callback ingestion and retain bounded playable-command storage; filters must precede source/admission caps.
- **596 — P2: category editor persists row changes immediately.** [PR596 src/ajustes_ux_interacao.inc:219](/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage/pr596/src/ajustes_ux_interacao.inc:219) writes the toggle; `ajustes_ux_ilha.inc:1164` labels OK as Confirm and Back as Close. This cannot provide staged Confirm/Cancel. **601 fixes it** at `ajustes_ux_interacao.inc:214` and `:322`; never ship 596's editor alone.
- **601, inherited by 602 — P2: full-category Confirm has quadratic main-thread work.** [PR601 src/guia.c:2554](/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage/pr601/src/guia.c:2554) and `:2562` traverse every draft entry; each calls [src/guia_categorias.inc:69](/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage/pr601/src/guia_categorias.inc:69), which rescans categories and recomputes source/name hashes. Even unchanged Confirm traverses both loops. At 30,000 categories, up to roughly 900 million category visits, before changed-entry linear override scans. Large-guide tests seed 3,000 groups but do not establish responsive bulk Confirm. Compare draft against its opening state and submit only changes; avoid repeated linear key lookup for bulk edits.
- **596/601/602 integration — P1 feature-loss risk if resolved by replacing whole files.** [PR602 src/guia.c:440](/Volumes/ExternalSSD/tmp/jobtmp/livepr-triage/pr602/src/guia.c:440) has no livefiltro quality/channel inventory admission; [livefiltro src/guia.c:522](/Volumes/ExternalSSD/nv-205-livefiltro/src/guia.c:522) owns those rules. Integration base also has category suggestion and pending-candidate handling (`src/guia.c:439`, `:1225`); raw PR code predates those parts. Three-way carries some automatically and leaves others conflicted. Preserve both contracts explicitly; never choose one side wholesale.

**587:** no concrete P0–P2 defect found in inspected scope propagation/persistence paths (`streams.c:404`, `:889`, `:2334`; `fontefalha.c:315`). Override is intentionally add-on-wide, including that add-on's movie/series streams; it must not disable livefiltro's guide quality/channel rules.

**597:** no concrete P0–P2 defect found in inspected recovery/watchdog paths (`LiveWindowRecovery.kt:30`, `:52`; `LivePlaylistDataSource.kt:40`, `:66`; `src/livestall.h:84`; `src/video_android.c:1134`). Kotlin/native signature changes must ship together. Acceptance is code-triage approval, not Android runtime certification; host build cannot validate Media3/JVM behavior. No P0 found.

## Overlap with livefiltro

| Area / shared functions | Better contribution; preserve both | Does stack solve livefiltro findings? |
|---|---|---|
| `sCanalAdd`, `lerPagina`, `carregarCatalogo`, `xtCanal`, `m3uCanal`, `stalkerCarregar` | livefiltro: All / Only 4K / Prefer 4K, duplicate replacement, filter-before-cap and retained variants. 596: account-aware loaders/IDs and M3U integration. | **No.** Large-guide storage tests do not prove filtered later pages remain reachable. |
| Per-source channel hide, profile persistence | livefiltro `guiafiltro_canal_oculto_fonte` / `guiafiltro_canal_alternar_fonte`: source key + exact ID, legacy wildcard/show migration. 596 adds stable native account IDs. | **No equivalent channel-hide feature.** Map old account-zero source keys when introducing account-qualified keys. |
| Category hide/editor | 596/601: account-qualified category keys, draft Confirm/Cancel and restoration from Settings. livefiltro: category filtering before visible cap; its category rule is name-based. | **Yes for Confirm/Cancel, partly for large lists.** Preserve legacy category scope via migration; presentation-only hiding must not consume filtered slots. |
| Inventory and pagination | livefiltro: 4,000-entry inventory, 64/category sample, 4 MiB bounds, paged category channel picker, raw cache. Stack: packed guide storage and increased source capacity. | **No for inventory bounds/picker pagination.** These are different structures, not replacement implementations. |
| Stalker truncation | Neither committed side resolves latest finding. Livefiltro owner is implementing callback scan now. | **No.** Stack retains 1,200/caller-prefix truncation. Re-review owner's final callback fix with multi-account commands. |
| `buscaFazer`, `buscaPasso`, modal input | 602: schedule batches limited to 4 ms / 256 pages, immediate copied titles, stable results and revision restart. | **Yes for EPG search responsiveness; not inventory pagination.** Channel-name scan remains synchronous, max 24 results; preserve existing limits. |
| Playback source filtering/recovery | 587 bypasses VOD source constraints; 597 handles Android/HLS stalls. livefiltro filters guide channel inventory. | Complementary. Neither PR replaces guide quality/hide work. |

## Integration order; feature preservation gate

1. Take **597**, then **587**. Both clean against pinned base and clean together. Retain both Kotlin recovery classes, native coordination, add-on override UI/persistence and mixed-list behavior.
2. Let livefiltro owner finish Stalker callback fix and commit it. Keep livefiltro as guide admission/inventory baseline; do not approve or overwrite its changing worktree.
3. Port **all 596 capabilities**, adapting loaders to livefiltro admission: per-profile multi-account migration/CRUD/order/enable/source selection, M3U metadata/headers/XMLTV, account-qualified playback/favorites/history/EPG/cache identity, responsive packed guide/loading/error/refresh behavior, EPG memory mode and private sign-out cleanup. Keep category suggestions, pending budgets, quality modes, variant replacement, exact source/channel hides, bounded inventory/raw cache and pagination. Account ID migration must preserve existing saved choices.
4. Port **601 increment**, fixing bulk Confirm complexity and integrating account-aware category draft with filter-before-cap. Port **602 increment** unchanged where possible; retain touch modal/keyboard ownership, batched EPG search, ordering, limits, pagination and cancellation.
5. Before release: verify all preserved features together, particularly hidden-prefix Stalker, higher-quality later variants, multiple same-provider accounts, large inventory restoration, account-zero migrations, failed save/Cancel/Back, repeated touch search and Android recovery/JNI. These integration fixes and tests are pending, not performed by this review.

## Verification

One target suite and one host build, in a fresh scratch copy of pinned base plus 597+587:

- `python3 /Volumes/ExternalSSD/tmp/rev/livepr/verify.py test` — **rc 0**; sequential applicability plus `tests/livestall_window.sh`, host / `NV_TPK` / `NV_ANDROID` variants.
- `python3 /Volumes/ExternalSSD/tmp/rev/livepr/verify.py build` — **rc 0**; `tools/build-local.sh`; existing `ATOMIC_VAR_INIT` deprecation warning.

Commands run from review worktree root; wrapper creates/uses scratch only. No network, installs, deployment, push, source edits or device screenshots. Guide-stack runtime/build remains unverified because conflicts were deliberately not resolved. Android JVM/device checks and 587-specific full regressions remain pending. Review document only committed.

**VEREDITO overall: BLOQUEIA** blind guide-stack integration. **587 ACEITAR; 596 PORTAR; 597 ACEITAR; 601 PORTAR; 602 PORTAR.**
