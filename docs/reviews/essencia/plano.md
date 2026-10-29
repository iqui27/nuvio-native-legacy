# 2.1 essence fix queue

Audit-only output. No job executed. Baseline `v2.0.8` (`87c8b8909`), audited HEAD `28b986b57`. See AUDITORIA.md for evidence, family IDs, scores and production entry points.

One job per screen/page family below. Guide v5 is one screen module with many subviews; keep a single owner to avoid conflicting edits. Sources AIO and regular Sources share one job. Server data views and wizard have different owners/visual responsibilities.

Every job: smallest component replacement; preserve functionality, routes, content, defaults, privacy and stored settings. No visual redesign. Use current exported helpers with 2.0.8 behavior; static baseline functions are implementation references, not public APIs to call across files. Do not create another UI toolkit. No push/deploy. Commit in English. No stash.

Verification per implementation job: one focused regression check and one local build; foreground commands with timeout. Capture settled + entry/mid-close/focus states, glass and solid, normal/reduced motion, default and a second accent. Check D-pad/Back/pointer and supported scale/rail settings. Unique drawings remain screen content. Never claim TV frame pacing from a host screenshot.

## Ranked jobs

| Order | Job / scope | Risk | CUSTOM % | Replacements | Stop condition |
|---:|---|---:|---:|---|---|
| 1 | Guide v5 | 88 | 89% | G1–G8 | All production subviews share baseline controls/motion; no feature removal. |
| 2 | Servers setup/management wizard | 85 | 83% | W1–W5 | Shared material/pills/rows/hints; entry/exit/focus interpolate. |
| 3 | Settings tour | 65 | 67% | T1–T4 | Shared cards/pills/typography, no white whole-card inversion. |
| 4 | Começar | 65 | 67% | C1–C4 | Shared neutral row focus and action pills; step/focus transitions. |
| 5 | Settings UX v4 | 65 | 67% | S1–S4 | Baseline material and real visible page/focus transitions restored. |
| 6 | Sources, including AIO | 55 | 50% AIO branch | A1–A3 | AIO metadata uses regular Sources component path and lifecycle. |
| 7 | Lists + trail/share | 40 | 25% | L1, L7 | Source badge shared; share overlay gains island lifecycle. |
| 8 | Achievements | 27 | 29% | Q1–Q2 | Hub honors glass/solid; focus neutral; badge art/motion kept. |
| 9 | Sources filters | 27 | 20% | F5 | Retained material gains open/close/category/focus animation. |
| 10 | Person History | 25 | 17% | H1 | Chapter selector shared; tab body transition retains continuity. |
| 11 | Add-on guide | 15 | 25% | D1, D8 | Shared kicker/progress; existing clock/springs retained. |
| 12 | Debrid sheets | 15 | 25% | D1, D8 shared implementation | No duplicate change if Add-on guide job already replaced kicker/progress. |
| 13 | Social/Friends | 15 | 0% new duplicates | No new custom replacement; inherited motion repair only | Existing visuals preserved; active-route exit/focus debt resolved if scheduled. |
| 14 | Filmography | 8 | 14% | P1 | Optional ring uses shared helper; all existing motion preserved. |
| 15 | Library + Origens | 8 | 13% | B1 | Origens heading/discs share primitives; current tabs/cards untouched. |
| 16 | AIO dashboard | 8 | 13% | I1 | State/progress indicators shared; existing animated controls retained. |
| 17 | Detail 2.1 | 7 | 11% | E1 | Rating surfaces/bars use baseline components; B2/motion intact. |
| 18 | Juntos lobby/sheets | 0 | 0% | None evidenced | Verification-only job; do not invent UI changes. |
| 19 | Servers Library/title/source views | 0 | 0% | None evidenced | Verification-only job; data routes stay on shared renderers. |
| 20 | Home 2.1 | 0 | 0% | None evidenced | Verification-only job; approved Home variants and motion preserved. |

## Job details

### 01 — Guide v5

Files: `src/guiav5ui.c`; animation integration only where required in `src/guia.c`.

| Current element | Replacement, exact file:function | Good baseline screen |
|---|---|---|
| `panel`, `gv_sheet_base`, modal/empty/error shells | `src/plrui.c:plrui_material`; `src/ilha.c:ilha_corpo` / `src/plrilha.c:plrilha_pedir` for sheets | Player islands / Sources |
| `top`, `gv_topbar` selection rail | `src/plrui.c:plrui_seg`, preserving current destinations | Player/Profile segmented control |
| `button`, `toggle`, `gv_button`, PIN/setup actions | `src/botoes.c:botao_pilula`, `botao_disco`; `src/plrui.c:plrui_botao_repouso`, `plrui_pilula_foco`, `plrui_tinta` | Detail buttons / player discs |
| `chip`, `gv_chip` | Same pill primitives for actions; `src/plrui.c:plrui_seg` for exclusive selection | Sources chips / player tabs |
| `gv_opt`, `gv_side_row`, schedule/option rows | `src/plrui.c:plrui_linha_foco` | Player selection rows |
| `gv_kb`, `gv_hints`, custom footer hints | `src/plrui.c:plrui_dicas` | Player footer |
| Catalog/sports/setup card surfaces | `src/plrui.c:plrui_cartao`, `plrui_linha_foco`; `src/focoprof.h:foco_zoom`, `foco_anel` only for artwork | Detail/Home |
| Boolean visibility/focus | `src/anim.h:anim_mola`, `anim_mola2_reduzida`; retained closing CLOCK body; shared `TXT_ILHA_*` via `src/text.c:txt_linha_corta` | Home/Detail/clock |

Cover main grid/cards/preview/dock/catch-up/categories, search/number/options/channel/settings/folders, EPG/health/error/empty, sports, IPTV setup/phone/source management, VOD title/series episodes, parental controls and multiview. Fix empty-source overlay dispatch without hiding configuration paths. Preserve EPG geometry, real previews and multiview panes. No screen consolidation or removed functionality.

### 02 — Servers setup wizard

Files: `src/srvsetup.c`. `servidores.c` stays data-only.

| Current | Replacement | Baseline |
|---|---|---|
| `panel` | `src/plrui.c:plrui_material`, `plrui_linha_foco` | Player islands/rows |
| `button` | `src/botoes.c:botao_pilula` / `src/plrui.c:plrui_pilula_foco` | Detail actions |
| `row` | `src/plrui.c:plrui_linha_foco` | Player list |
| `dicaChip` | `src/plrui.c:plrui_dicas` | Sources footer |
| Selected step labels | `src/plrui.c:plrui_seg` for selection; `plrui_trilho` for completion | Player tabs/progress |
| Immediate `opened` / `selected` | `src/anim.h:anim_mola`, `anim_mola2_reduzida`; `src/movimento.h:mov_mola_assenta` for modal shape if used | Detail / clock modal |

Preserve discovery, credentials, Quick Connect, Plex/Emby Connect, library selection, ready/remove and errors. Verify smaller virtual canvas and fixed rail. QR contrast backing remains white.

### 03 — Settings tour

Files: `src/ajustes_ux_tour.inc`, narrowly scoped animation state in Settings update.

| Current | Replacement | Baseline |
|---|---|---|
| `tourCartao` white inverted card | `src/plrui.c:plrui_cartao`, `plrui_linha_foco` | Detail cards |
| `tourBotao` | `src/botoes.c:botao_pilula` | Detail/player actions |
| NEW/header raw status tiles | `src/plrui.c:plrui_botao_repouso`; configured accent for state; `src/text.c:txt_linha_corta` with `TXT_ILHA_HORA` | Sources informational chips |
| Tour custom text hierarchy | `src/plrui.c:plrui_kicker`; `src/text.c:txt_bloco`, `txt_linha_corta` with `TXT_ILHA_*` | Settings headers/rows |
| Boolean focus/visibility | `src/anim.h:anim_mola` and retained page exit; preserve existing `anim_mola2` scroll | Home/Detail |

Keep functional routing, “Where is it”/enable controls, capability availability and real `tourPrevia` scenes. Do not replace previews with generic illustrations.

### 04 — Começar

Files: `src/comecar.c`.

| Current | Replacement | Baseline |
|---|---|---|
| `superficie` accent-filled rows/cards | `src/plrui.c:plrui_material`, `plrui_linha_foco`; only actions use `src/botoes.c:botao_pilula` | Settings rows/player buttons |
| `bola` raw icon tile | `src/plrui.c:plrui_material`, `src/botoes.c:botao_disco` when interactive | Player icon controls |
| `cabecalho` raw step bars/header | `src/plrui.c:plrui_trilho`, `plrui_kicker`; `src/text.c:txt_linha_corta` with `TXT_ILHA_*` | Player progress/header |
| Local swatch focus ring | `src/botoes.c:botao_disco` / `src/plrui.c:plrui_pilula_foco` around actual swatch; `src/anim.h:anim_mola` | Player discs |
| Binary focus/step switch | `src/anim.h:anim_mola`; retain outgoing step for fade, retain existing reduced-motion page ramp | Home/Detail |

Preserve four steps, account import and sync semantics, avatar colors and tracker choices. No new authentication behavior. Swatch color is user content, not a fixed theme accent.

### 05 — Settings UX v4

Files: `src/ajustes_ux_v4_desenho.inc`, minimal state/update in `src/ajustes_ux_v4.inc` / `src/ajustes.c`.

| Current | Replacement | Baseline |
|---|---|---|
| `aj4Pagina` own dense shell/veil | `src/ajustes_ux_desenho.inc:ajIlha` / `src/plrui.c:plrui_material` | Baseline Settings islands |
| `aj4LinhaDesenhar` boolean surface | `src/ajustes_ux_desenho.inc:ajFocoLinha` with interpolated focus | Baseline Settings rows |
| `aj4Botao` and manually composed modes | `src/botoes.c:botao_pilula`; `src/plrui.c:plrui_seg` | Detail buttons/player tabs |
| Footer literal hints and custom hierarchy | `src/ajustes_ux_desenho.inc:ajDicasLinha`; `src/plrui.c:plrui_kicker`; shared `TXT_ILHA_*` | Settings footer/header |
| Unused legacy `paginaA` animation | Consume appropriate page/focus animation in active v4 draw tree via `src/anim.h:anim_mola` / baseline ramp, retaining old body through close | Existing Settings/Detail |

Keep RTL mirroring, Simple/Advanced, search, presets, difference review/undo, help and profile editor semantics. Preserve all option defaults/migrations and actual preview content. Do not revert functional UX reorganization.

### 06 — Sources, regular + AIO

Files: `src/streams.c`.

| Current | Replacement | Baseline |
|---|---|---|
| `corpoAio` own provider tabs | `src/plrui.c:plrui_seg` with interpolated selection/focus | Player tabs |
| `corpoAio` fixed-color focus rows and index-window jump | Reuse `src/streams.c:corpoFolha`, `focoFonte` structure; `src/plrui.c:plrui_linha_foco`; `src/anim.h:anim_mola2_reduzida` | Regular Sources |
| `corpoAio` own group/title/footer/state rendering | Existing regular `corpoFolha` header/state branches, `src/plrui.c:plrui_kicker`, `plrui_dicas` | Regular Sources |
| Outside-player `temAio()` bypass of island | `src/ilha.c:ilha_corpo`, preserve `src/plrilha.c:plrilha_pedir` in player | Clock Sources/player sheets |

Keep AIO actions/counts/services, actual source order, badges, MP4/cache/dub filters and selection. Empty, loading, failure and all-filtered outcomes must use same state path. Preserve regular Sources. Avoid a second copied row renderer; adapt data/content through existing path.

### 07 — Lists / trail / sharing

Files: `src/biblioteca.c:desenhaCartaoLista` badge only.

Replace L1 raw black provenance badge with `src/plrui.c:plrui_botao_repouso` and `src/text.c:txt_linha_corta` using `TXT_ILHA_HORA`. Baseline: Sources capability badges. Badge stays informational, compact, nonfocusable; do not turn it into a 56px action. Keep only useful source provenance and actual attribution. Replace L7 `desenhaCompartilhar` full-screen .82 veil and abrupt visibility with `src/ilha.c:ilha_corpo`, `src/plrui.c:plrui_material`, shared text/hints and retained closing body. Baseline: Sources clock sheet. Keep QR/URL content. Shared card/tab/row motion already works. Preserve mine/public/fixed/collections, list history/curator notes, trail order and states. Coordinate job 15 in same file.

### 08 — Achievements

Files: `src/conquistas.c`.

| Current | Replacement | Baseline |
|---|---|---|
| `superficie` always-solid hub/retomar/map/family cards | `src/plrui.c:plrui_material` / `plrui_cartao` | Player/Detail cards |
| Local white wash + `desenha_card` colored focus overlay | `src/plrui.c:plrui_linha_foco` with existing `focos[]` spring | Player rows |

Retain trophy artwork, progress/level semantic colors, current spring detail modal, notice, tabs, buttons, scrolling and stagger. Verify reduced motion; no gamification redesign.

### 09 — Sources filters

Files: `src/ajustes_ux_ilha.inc`, narrow lifecycle/update state in Settings. Inherited motion debt; preserve current material.

Replace binary whole-sheet/category lifecycle F5 with `src/ilha.c:ilha_corpo` or existing `src/movimento.h:mov_mola_assenta` central-modal lifecycle; use `src/anim.h:anim_mola` for row focus and `anim_mola2_reduzida` for scroll. Baseline reference: clock/player sheets.

Retain `ajIlha`, `ajCabecalhoFolha`, `ajFocoLinha`, `ajPilulaAcento`, exclusion semantics and immediate save. Do not change filter meanings. Central modal is allowed; no requirement to move every modal into clock if existing centered layout is retained with correct lifecycle.

### 10 — Person History

Files: `src/filmo.c` History branch only. Coordinate shared file ownership with job 14.

Replace chapter-selection composition H1 with `src/plrui.c:plrui_seg`, preserving actual chapter/year data. Reference: player/Profile tabs. Connect existing `src/anim.h:anim_mola` state to outgoing/incoming tab bodies, preserving current spring year/scroll/poster reveal. Keep `ilha_corpo` overlays. No new timeline feature or content.

### 11 — Add-on guide

Files: `src/addonguia.c:kicker` only, plus coordinate adjustment if needed.

Replace D1 manual uppercase/tracking with `src/plrui.c:plrui_kicker`, inside correct existing text scale. Replace D8 two raw progress rectangles (`corpoDesenhar:670–672`) with `src/plrui.c:plrui_trilho`, retaining unknown-progress behavior. Baseline: Sources header. Keep `botao_pilula`, shared line focus, `plrilha_pedir`, current retained body/reveal, all guide/mode/list/failure/phone flows. Verify resulting text metrics in long translations; no sheet redesign.

### 12 — Debrid sheets

Files: `src/addonguia.c` AG_CONECTAR / AG_DEBRID branches.

Duplicates D1/D8: replace with `src/plrui.c:plrui_kicker` and `src/plrui.c:plrui_trilho`, baseline Sources header/player progress. If Add-on guide job already changed shared kicker/progress, this job makes no code edit: verify connection/preparing/phone/offline branches and exit/focus motion. Keep `plrui_material`, `botao_pilula`, clock-centered `plrilha_pedir` and progress. No new Debrid account/auth behavior.

### 13 — Social / Friends

No new CUSTOM replacement identified. Default: verification-only; inherited motion follow-up if central schedules repair.

Active route is `app.c` TELA_SOCIAL to `amigoperfil_desenhar`, not `social_desenhar`. Keep `src/svdesenho.c:svd_*`, `pessoas.c` shared row/button/material behavior, Home Friends normal focus. For inherited abrupt exit/binary row focus only: `src/anim.h:anim_mola`, retained closing body in `amigoperfil.c` and `amigostitulo_ui.c`. Reference: player/clock sheets and Home/Detail springs. Preserve private/disconnected/stale states and permissions. Do not rewrite working social visuals or add disallowed Home Friends actions.

### 14 — Filmography

Files: `src/filmo.c:cartaz`.

Replace P1 copied poster border implementation with `src/focoprof.h:foco_anel`; keep existing `foco_zoom` and `src/home.c:home_vitrine_cartao`. Baseline: Home poster rows. Respect current helper radius convention; verify non-square poster dimensions and optional border both on/off. Preserve grid filters, year chips, About/Conquests and clock title/quiz/date/biography sheets. No blanket rewrite of new person page.

### 15 — Library + Origens

Files: `src/biblioteca.c:desenhaOrigens`.

Replace B1 manually tracked Origens heading with `src/plrui.c:plrui_kicker`. Use `src/plrui.c:plrui_material` for informational icon/disc surface; `src/botoes.c:botao_disco` only when that disc is a real action. Baseline: player icon/row headers. Keep actual provider logos/letters; neutral row focus already uses `plrui_linha_foco`. Preserve `plrui_seg_ex`, spring tabs/cards/scroll, origin selection, retry/empty state and safe margins. Shared list source branch may use same function; coordinate job 07.

### 16 — AIO dashboard

Files: `src/aiostreamsui.c` state indicators only.

Replace I1 step/status/progress drawings with `src/plrui.c:plrui_trilho`, `plrui_respira`, `plrui_botao_repouso` plus `TXT_ILHA_HORA`; dynamic accent for active state. Use existing `src/ilha.c:ilha_corpo`/app island clock ownership where a clock surface is needed; do not draw another independent fixed-format clock. Baseline: player progress/clock and Sources state chips. Keep semantic error/warning distinction, all current shared rows/buttons/panels and page/focus/reveal motion. Preserve setup, overview/catalogs/services/links/subtitles/read-only filter/health/title inspectors and CAS/version constraints.

### 17 — Detail 2.1

Files: `src/detail.c:desenhaMediaTile`, `desenhaNotasTiles`.

| Current | Replacement | Baseline |
|---|---|---|
| Raw rating tile wash | `src/plrui.c:plrui_cartao` | Detail cast/trailer/More cards |
| Own-note decorative outline / duplicated focus treatment | `src/plrui.c:plrui_linha_foco`, retain existing `animFoco` | Player/Detail surfaces |
| Raw paired progress/comparison bars | `src/plrui.c:plrui_trilho` with honest numeric fractions | Player progress |
| Local label hierarchy | `src/text.c:txt_linha_corta` shared styles, existing numeric renderer where needed | Detail metadata / Sources |

Keep actual critic/public/source/friends/own ratings, semantic comparisons, hero actions, B2 spacing, episodes, cast, all current navigation/motion and optional poster border. Do not change header layout because older comments conflict; inspect current owner-approved behavior.

### 18 — Juntos lobby and sheets

Verification-only. No replacement justified. Check `src/juntosui.c:desenharLugar`, `desenharSala`, `pedirCorpo`, `desenharCorpo`: shared `plrui_linha_foco`, `botao_pilula`, `ilha_corpo`/`plrilha_pedir`, spring focus/scroll, countdown and retained close. Keep intentionally solid cinema seat cards, unique theater/reaction art. If validation exposes clipping, fix geometry at narrowest layer; do not invent aesthetic work.

### 19 — Servers Library / title / source views

Verification-only. No replacement justified. Trace `servidores.c` data into `biblioteca.c`, `detail.c`, `streams.c`. Validate actual local provider data, missing art, loading/empty/error, selection and return focus. Retain baseline cards/actions/source rows/motion. Wizard style belongs exclusively to job 02.

### 20 — Home 2.1

Verification-only. No replacement justified. Baseline `home.c` poster/hero/Stage/Cover Flow/large/round/editorial cards and `desenhaChipAmigos` remain. Validate entry/exit/Detail return, row focus/reveal/scroll, empty QR and missing-art/loading states, each approved layout. Keep overlap/layout and optional focus border. No generic card replacement or new Home design.

## Integration boundaries

| Shared file / owner overlap | Ordering |
|---|---|
| `guiav5ui.c` | One Guide job owns all subviews. |
| `biblioteca.c` | Library Origens first, Lists badge after; minimal nonoverlapping edits. |
| `filmo.c` | History first, Filmography helper cleanup after. |
| `addonguia.c` | Add-on guide kicker first; Debrid verifies same shared fix. |
| Settings includes | v4/tour/filters share update state; merge independently reviewed changes before final combined motion test. |
| `streams.c` | One Sources job owns AIO + regular variants; do not let dashboard job fork source-row rendering. |

No UI fixes, captures, push or deployment performed by this audit. A separate diary/history screen beyond person História remains unproven; central must identify its production route before adding scope.
