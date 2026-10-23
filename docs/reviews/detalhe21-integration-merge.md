# Detail integration merge

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
