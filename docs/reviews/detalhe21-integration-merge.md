# Detail check follow-up

The ten inherited failures were investigated against the approved fixed Detail layout. No tests were deleted. Production changes are limited to cancelling a pending cast lookup on Back and resuming an in-progress season-zero special. Neither changes drawing or layout.

| Check | Classification and retained protection |
| --- | --- |
| `detail_epcard.sh` | Stale size/API expectations; fixed 420×530 cards, 236 px thumbnail, 448 px pitch, all Home layouts and legacy size preferences, full row navigation. |
| `detail_epinfo.sh` | Stale Modern C panel and hole API, plus duplicate embedded rewatch implementation in the link; current title, absent duration/date, three-line synopsis bounds, all Home layouts and both glass settings captured. |
| `detail_episode_perf.sh` | Stale cast-relative viewport comparison and removed hole API. Approved episode viewport includes hero/season controls while the cast viewport does not. Explicit ceiling: 220 submissions and 1.15 screen fills; measured maximum 204/1.126, zero navigation cache churn across 5,120 frames. Alpha-zero, rating measurement, text-weight pixels and frost checks remain. |
| `detail_episode_review.sh` | Stale renderer guards; retains highp text, dither, scaled rect visibility, focus/page alpha, filter/navigation/rating wiring and executable portrait material checks. |
| `detail_epmaterial_shot.sh` | Stale hole API, still-pixel invariance and settings migration fixture; validates fixed geometry, visible focus changes confined to the scaled thumbnail, and Modern/Standard captures. |
| `detail_eps.sh` | Stale global source guards: progress-grid columns are intentionally absolute; episode-card columns remain season-relative. Initial selection now routes through `episodioPadrao`. |
| `detail_eps_shot.sh` | Stale hole API, hidden/offscreen blur targets, unpinned fixture season and old source-strip height. Explicitly scrolls both unseen episodes into view and retains watched/unknown blur, source metadata and native captures. |
| `detail_epslista.sh` | Stale scaled-gap API plus real regression: season-zero resume was rejected by `temporada > 0`. Retains special selection/resume, first regular fallback, spring, reduced motion and stable neighbors. |
| `detail_layout.sh` | SIGABRT/rc 134 was an assertion expecting the retired collapsed two-button group. Ports direct List/More actions. Subsequent cancellation assertion exposed a real regression: Back left cast lookup pending. Fix cancels it before navigating; worker isolation/cancellation/cache tests remain. |
| `detail_secoes_shot.sh` | SIGABRT/rc 134 was `colListaAberta && colListaFoco == 1`: retired collection-list behavior. Approved row opens each part directly; fixture asserts current-part no-op and another part's exact TMDB routing, retaining all section captures. |

Final verification: **25/25 `tests/detail*.sh` commands returned 0**, as did `python3 tools/varredura-i18n.py`, `python3 tools/idiomas.py`, `python3 tools/detalhe21-layout-png.py` and `bash tools/build-local.sh`. Every originally failing check listed above now returns 0. No tests were removed.

All **21 PNGs are pixel-identical** to the saved pre-task contents of `/Volumes/ExternalSSD/tmp/rev/detalhe21-layout-png/`, copied to `/tmp/detail21-resto-png-before/` before capture. ImageMagick `compare -metric AE` returned 0 and zero differing pixels for every file, including all four full pages and all comparison crops. Initial differences were exclusively estimated-finish clock labels (baseline 08:41 versus current wall clock); the capture fixture now accepts `NUVIO_SHOT_TIME`, and the generator fixes that clock and timezone to the saved reference. Production rendering was not changed. Output remains `/Volumes/ExternalSSD/tmp/rev/detalhe21-layout-png/`.

No push, deployment, stash, or TV validation. Host draw/fill budgets do not establish TV performance.

---

# Current merge verification

Merged `integracao/2.1` at `147263c2c`, retaining 2.0.8 fixes and Settings v4. Detail-only settings follow integration IDs in the enum, options, keys and defaults; existing section order is preserved. Both Detail options route to the title-page group. All 30 language tables retain both parents' keys, including `Trilha`; incoming Arabic translations win shared-key conflicts.

Restored integration's #595 title-bound watched-request guard. Updated action-spacing and cast fixtures for the approved layout and current quick-glance entry; supplied missing metadata fixture stubs. No stash, push or deployment.

Final verification: **21 pass, 10 fail**. Ratings, fullscreen trailer fallback, Settings v4, Detail 2.1, action spacing, watched-request isolation, Mac compilation, cast memory/navigation, all four episode-source checks, stored episode evidence, metadata re-request, remount, PNG generation, both language checks and local build returned 0.

Remaining failures:

- rc 1: `bash tests/detail_epcard.sh`, `bash tests/detail_epinfo.sh`, `bash tests/detail_episode_perf.sh`, `bash tests/detail_episode_review.sh`, `bash tests/detail_epmaterial_shot.sh`, `bash tests/detail_eps.sh`, `bash tests/detail_eps_shot.sh`, `bash tests/detail_epslista.sh`.
- rc 134: `bash tests/detail_layout.sh`, `bash tests/detail_secoes_shot.sh`.

These checks reference older episode geometry/APIs (`epFuros`, `epEscala`, `NV_DETP_EPM_*`), old source guards, or old button/collection behavior. They remain unresolved; this merge does not claim a green full Detail suite.

Regenerated PNGs preserve the pre-merge layout. `series-pronta.png` and `notas-media.png` are pixel-identical to saved pre-merge captures; other full pages differ by less than 0.01% of pixels. Existing differences from the original mock (fixture content, average-rating summary, season shuffle and spoiler state) remain. Output: `/Volumes/ExternalSSD/tmp/rev/detalhe21-layout-png/`. After the external SSD filled, capture scratch directories `pronta` and `pessoal` moved to `/tmp` with symlinks preserving output paths.

---

# Previous Detail integration merge

The approved detail layout is retained while integrating the 2.0.8 fixes and current `integracao/2.1` through `c77a2e59d8ed26c717e93987193d2eeab79b7486`.

The worktree initially contained an unfinished merge of `91f4219ec`. That merge was completed first, followed by a merge of the current integration tip. No stash, push, or deployment was used.

## Conflict resolutions

| File | Resolution |
| --- | --- |
| `src/ajustes.c` | Keep detail hero/cast settings and incoming AFR/hero-size settings, matching enum, option, key, and default positions. Retain incoming accessors, platform restrictions, and persistence behavior. |
| `src/ajustes_ux_padrao.inc` | Keep both detail defaults and incoming AFR Off/hero size Normal defaults in enum order. |
| `src/detail.c` | Keep approved hero anchors, compact rows, spoiler controls, personal rating UI, and average rating summary. Integrate cache-compatible loading, half-star rating backend/episode badges, music page, cast lookup, source/provider routing, comment navigation, touch handling, and rewatch actions. `episodioPadrao()` alone chooses the default: rewatch, explicit server episode, in-progress, next unwatched, first regular episode. Explicit row selection remains separate. Remove an empty duplicate info-tab branch. In the second merge retain server/friends navigation and provider spacing while keeping the approved hero and source-description layout. |
| `src/detail.h` | Combine server source accessor and episode source-request API with the branch's detail interface. |
| `src/idioma_tab.h` | Union both parents' English string keys; retain branch detail strings and incoming strings in byte order. |
| `src/idioma_ar.h` | Union Arabic entries from both parents. |
| `src/idioma_bg.h` | Union Bulgarian entries from both parents. |
| `src/idioma_bs.h` | Union Bosnian entries from both parents. |
| `src/idioma_cs.h` | Union Czech entries from both parents. |
| `src/idioma_da.h` | Union Danish entries from both parents. |
| `src/idioma_de.h` | Union German entries from both parents. |
| `src/idioma_el.h` | Union Greek entries from both parents. |
| `src/idioma_es.h` | Union Spanish entries from both parents. |
| `src/idioma_fr.h` | Union French entries from both parents. |
| `src/idioma_hu.h` | Union Hungarian entries from both parents. |
| `src/idioma_id.h` | Union Indonesian entries from both parents. |
| `src/idioma_it.h` | Union Italian entries from both parents. |
| `src/idioma_ja.h` | Union Japanese entries from both parents. |
| `src/idioma_lt.h` | Union Lithuanian entries from both parents. |
| `src/idioma_nl.h` | Union Dutch entries from both parents. |
| `src/idioma_no.h` | Union Norwegian entries from both parents. |
| `src/idioma_pl.h` | Union Polish entries from both parents. |
| `src/idioma_ptpt.h` | Union European Portuguese entries from both parents. |
| `src/idioma_ro.h` | Union Romanian entries from both parents. |
| `src/idioma_ru.h` | Union Russian entries from both parents. |
| `src/idioma_sk.h` | Union Slovak entries from both parents. |
| `src/idioma_sl.h` | Union Slovenian entries from both parents. |
| `src/idioma_sr.h` | Union Serbian entries from both parents. |
| `src/idioma_sv.h` | Union Swedish entries from both parents. |
| `src/idioma_tr.h` | Union Turkish entries from both parents. |
| `src/idioma_uk.h` | Union Ukrainian entries from both parents. |
| `src/idioma_vi.h` | Union Vietnamese entries from both parents. |
| `src/idioma_zhcn.h` | Union Simplified Chinese entries from both parents. |
| `src/idioma_zhtw.h` | Union Traditional Chinese entries from both parents. |

All translation tables were checked for preservation of every key from both merge parents.

## Verification

All six final commands returned exit code 0, run from the worktree root:

- `python3 tools/varredura-i18n.py`
- `bash tools/build-local.sh`
- `bash tests/notasfontes.sh`
- `bash tests/issue674_play_button.sh`
- `bash tests/detail_notas.sh`
- `python3 tools/detalhe21-layout-png.py`

The #674 test includes rewatch and Jellyfin/Emby/Plex default-target cases. The comments test fixture now supplies both series and episode comments, so its focus survives column synchronization. The screenshot fixture waits for asynchronous texture uploads and artwork fades before capture; an earlier capture completed while artwork was still decoding. The production layout is unchanged by these fixture fixes.

## PNG locations

- Saved pre-merge captures: `/Volumes/ExternalSSD/tmp/rev/detalhe21-approved-before-merge/`
- Approved mock reference: `/Volumes/ExternalSSD/tmp/detalhe21/v2/pagina-completa-pronta.png`
- Regenerated captures and side-by-side comparisons: `/Volumes/ExternalSSD/tmp/rev/detalhe21-layout-png/`
- Full pages: `movie-pronta.png`, `movie-pessoal.png`, `series-pronta.png`, `series-pessoal.png`
- Average rating detail: `notas-media.png`
- Comparison crops: `14-hero.png` through `23-progress.png`.

Visual inspection of the regenerated movie and series pages confirms the approved layout, including hero anchors, row positions, compact collection cards, and average rating summary. All 22 captured tiles finished with zero pending textures. The rating-summary, comments, footer, and progress comparisons preserve their existing content; artwork and time-dependent text prevent whole-page byte equality. No TV deployment or device validation was performed.
