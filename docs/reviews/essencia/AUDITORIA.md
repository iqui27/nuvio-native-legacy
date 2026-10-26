# 2.1 essence audit

Audit only. No UI edits. Snapshot HEAD `28b986b57ed6091468c3e9173199ecb628b7f178`, branch `agente/21-21-essencia`.

Owner intent: retain 2.0.8 component quality and motion; fix bugs. New functionality is not permission to replace visual grammar.

## Evidence and scoring

| Reference | Evidence / interpretation |
|---|---|
| Quality baseline | `v2.0.8` = `87c8b8909f0d605f1d8351147fe5afb9de233b8d`. All replacement functions below exist there. Current signatures can differ. |
| Required history | Read `git log integracao/2.0.5`; inspected visual history including `790abb451` (accent focus independent of glass), `e4d901a1f` (hero focus glass pill), `601ceec5b` (Saved panel focus), `3cf9a95af` and `8c3187593` (preserve B2 detail/C9 glass/spacing). Current `integracao/2.0.5` = `6b9683db5ec085e5169d944b9c344addfec5bfe7`; it is not a frozen 2.0.8 release. |
| Owner constraints | Read DECISOES, DESIGN, MOCKUPS, PRODUCT. Preserve B2, approved Home layouts, dynamic accent, glass/solid geometry, optional poster border, reduced motion. Do not treat every `design/2.1` prototype as owner approval. |
| Scope | Production draw routes, helper implementations, animation updates and baseline provenance. No captures, runtime navigation, pixel comparison or TV timing. Scores measure source-level duplication, not measured pixels or visual attractiveness. |
| Unit | One named visual family per inventory row. Repeated instances count once. Rows explicitly split where one implementation changes materially. CUSTOM means new duplicated UI drawing. REUSES-2.0.8-COMPONENT includes retained baseline screen drawing and shared primitives; provenance noted. Unique illustration/content stays with screen and is excluded from denominator. |
| Custom % | CUSTOM rows / classified rows, rounded to nearest whole %. Small denominators are intentional, not precision claims. |
| Motion | Overall yes = entry/exit/focus paths evidenced; partial = at least one gap or materially different path; no = no local transition/focus interpolation found. O/C/F columns separate open/close/focus. Existing baseline gaps are not new regressions. |
| Risk /100 | `round(custom% * .6 + motion penalty + structural penalty)`, capped at 100. Motion penalty: yes 0, partial 15, no 25. Structural penalty: 10 for new duplicated screen/sheet shell or new whole-row/card focus inversion; otherwise 0. Rank by risk, ties by reach. Not a release verdict. |
| Signals | T = missing local transition; C = fixed interactive/state colors; F = divergent text styles; R = own focus/ring; M = fixed geometry needs safe-area verification; E = missing explicit empty/loading branch. Fixed neutral colors alone are not a defect. M is a risk, not proof of visible misalignment. |

## Worst first

| Rank | Screen / sheet | CUSTOM / families | Custom % | Motion | Risk | Thrown-in signals / state evidence |
|---:|---|---:|---:|---|---:|---|
| 1 | Guide v5, all subviews | 8/9 | 89% | no | 88 | T C R M; repeated custom controls, fixed 1920 canvas. Empty/error views exist; not E. |
| 2 | Servers setup / management wizard | 5/6 | 83% | no | 85 | T R M; boolean selection, own hints/material/buttons. Network/loading/error handling exists. |
| 3 | Settings tour | 4/6 | 67% | partial | 65 | T C F R; white whole-card inversion, red NEW badges, own pills. Scroll spring only. Static cards: no network loading needed. |
| 4 | Começar, four steps | 4/6 | 67% | partial | 65 | C F R; accent-filled rows, local swatch ring, binary focus. Whole-page fade exists; step transition absent. |
| 5 | Settings UX v4 | 4/6 | 67% | partial | 65 | T F R M; own shell, tabs/rows/hints. Shared accent helpers retained, but boolean focus. |
| 6 | Sources with AIO detected, outside player | 3/6 | 50% | partial | 55 | C R M E; clock growth bypassed, custom provider tabs/rows; zero-result path lacks regular Sources state rendering. Slide exists. |
| 7 | Lists + trail + share sheets | 2/8 | 25% | partial | 40 | T F; source label and abrupt share overlay (.82 veil) are custom. Shared tabs/material/row focus and Library motion remain. Loading/empty branches present. |
| 8 | Achievements hub / detail / notice | 2/7 | 29% | yes | 27 | C R; hub surfaces always solid, added colored focus overlay. Modal/material and motion survive. Empty branch exists. |
| 9 | Sources filters sheet | 1/5 | 20% | partial | 27 | T; shared Settings material/rows retained; opening, close and category replacement immediate. Baseline debt, not new 2.1 regression. |
| 10 | History tab / person-story page | 1/6 | 17% | partial | 25 | T; custom chapter selection; year/card springs exist, tab body replacement lacks retained outgoing body. |
| 11 | Add-on guide / mode / list / failure sheets | 2/8 | 25% | yes | 15 | F; duplicated kicker and progress tracks. Clock-centered sheet, button/row springs, loading/failure branches present. |
| 12 | Debrid connect / preparing sheets | 2/8 | 25% | yes | 15 | F; same duplicated kicker/progress as add-on guide. Shared button/material/clock motion; offline and preparing branches present. |
| 13 | Social / Friends / activity | 0/8 | 0% | partial | 15 | Active friend profile has baseline abrupt exit; title-friends sheet has binary row focus. Retained debt; activity path improved. |
| 14 | Filmography grid / About / Conquests / actions | 1/7 | 14% | yes | 8 | R M; copied optional poster ring, fixed About columns. Shared cards, springs, clock sheets, loading/empty branches retained. |
| 15 | Library + Origens | 1/8 | 13% | yes | 8 | M; new Origens disc drawing; TV tabs/rows already shared. Safe-area helpers and empty/loading/error exist. |
| 16 | AIO dashboard + setup + inspectors | 1/8 | 13% | yes | 8 | C M; own status/progress indicators and clock; shared controls/motion already present. Busy/error/manifest-only branches exist. |
| 17 | Detail 2.1 + overlays | 1/9 | 11% | yes | 7 | C R; new ratings tiles duplicate surfaces/bars; own-note decorative outline. Main hero/episodes/cards/motion retained. |
| 18 | Juntos lobby + associated sheets | 0/7 | 0% | yes | 0 | Fixed cinema composition needs runtime scale checks; illustration is unique content. Shared seats/rows/buttons/clock and focus springs. |
| 19 | Servers Library / title / source views | 0/7 | 0% | yes | 0 | Uses Library, Detail and Sources renderers; `servidores.c` is data, not a second UI. Wizard ranked separately. |
| 20 | Home 2.1 | 0/8 | 0% | yes | 0 | No new duplicate visual family identified. Home row/chip changes retain baseline drawing. Loading/missing-art/empty QR routes exist. |

Sort by risk; ties prioritize broader production reach. Inherited-only debt is explicitly labeled. No claim that every screen needs redesign.

## Replacement catalog: exact baseline functions and good baseline screens

Paths/functions in this catalog refer to `v2.0.8`. Static functions are references within their module, not exported APIs. Prefer existing exported primitive; never copy an entire old renderer to another file.

| Key | Exact component file:function | Baseline screen where treatment is established |
|---|---|---|
| MATERIAL | `src/plrui.c:plrui_material`, `plrui_material_s`, `plrui_cartao` | Player Audio/Subtitles islands; Detail More/cards; Sources |
| ROW | `src/plrui.c:plrui_linha_foco` | Player selection rows; Friends title sheet |
| PILL | `src/botoes.c:botao_pilula`, `botao_disco`; `src/plrui.c:plrui_botao`, `plrui_botao_repouso`, `plrui_pilula_foco`, `plrui_tinta` | Detail/hero buttons and player sheets |
| SEG | `src/plrui.c:plrui_seg` | Player segmented controls; Profile friend selector. Current `plrui_seg_ex` is a 2.1 extension, not a baseline function. |
| HINT | `src/plrui.c:plrui_dicas`; Settings-local `src/ajustes_ux_desenho.inc:ajDicasLinha` | Player sheet footer; Settings footer |
| KICK | `src/plrui.c:plrui_kicker` | Sources header and player sheets |
| PROGRESS | `src/plrui.c:plrui_trilho`, `plrui_barra`, `plrui_anel`, `plrui_respira` | Player progress/loading/activity |
| CLOCK | `src/ilha.c:ilha_corpo`; `src/plrilha.c:plrilha_pedir` | Sources outside playback; player Audio/Subtitles/Episodes. Retains last body while shrinking. |
| POSTER | `src/focoprof.h:foco_zoom`, `foco_anel`; `src/home.c:home_vitrine_cartao` | Home poster rows / rest showcase. Keep user's optional border choice. |
| SETTINGS | `src/ajustes_ux_desenho.inc:ajIlha`, `ajFocoLinha`, `ajNeutro`, `ajPilulaAcento`; `src/ajustes_ux_ilha.inc:ajCabecalhoFolha` | Existing Settings islands/editors and filters |
| MOTION | `src/anim.h:anim_mola`, `anim_mola2_reduzida`; `src/movimento.h:mov_mola_assenta`; `src/revela.h:revela_entra` | Home/Detail focus and scroll; clock modal growth/reveal |
| SOURCE | `src/streams.c:corpoFolha`, `chipFolha`, `focoFonte` | Regular Sources sheet; preserve real badges, grouping and states |
| SOCIAL | `src/svdesenho.c:svd_avatar`, `svd_poster`, `svd_rosto`, `svd_chip` | Friends row, friend profile and social surfaces |
| TEXT | `src/text.c:txt_linha_corta`, `txt_bloco`; styles in `src/text.h` (`TXT_ILHA_*`) | Sources/Settings headers, rows and support text |

## Component inventory

R = **REUSES-2.0.8-COMPONENT**; C = **CUSTOM**. Each row's element list exhausts its visual family, not every repeated instance or glyph. O/C/F = open / close / focus: yes, partial, no, or — for noninteractive content. Replacement keys resolve to exact file:function above. Unique imagery/diagrams excluded from counts are listed afterward.

### Guide v5 (G1–G9)

Active route: `src/guia.c:guia_abrir` opens `g5ui_open("grade")`; `guia_desenhar` returns through `g5ui_draw` (`guia.c:3051,3060,7564`). Legacy `guia.c` motion does not animate v5.

| ID | Element / current drawing code | Class / actual dependency | O/C/F | Exact replacement / baseline reference |
|---|---|---|---|---|
| G1 | Main/setup shells and all sheet bases: `guiav5ui.c:352 panel`, `1592 gv_sheet_base`, `691 g5ui_draw` | C; raw glass/solid rects | no/no/— | MATERIAL + CLOCK; player sheets |
| G2 | Top tabs and navigation: `top`, `gv_topbar` (374,1164) | C; selected rects, binary `plrui_pilula_foco` | no/no/no | SEG + MOTION; Profile/player segmented control |
| G3 | Buttons/toggles: `button`, `toggle`, `gv_button` (356,368,1130) | C; small buttons own rects; large branch uses PILL | no/no/no | PILL with MOTION; Detail buttons |
| G4 | State/provider/filter chips: `chip`, `gv_chip`, `gv_chips_row` (369,1114,1191) | C; white selected fill and fixed ink | no/no/no | PILL for actions, SEG for exclusive selection; Sources chips |
| G5 | Sidebar, option/settings/folder/EPG rows: `gv_side_row`, `gv_opt` (1221,1599) | C; bright inverted row | no/no/no | ROW + MOTION; player selection rows |
| G6 | Footers: `top`, `gv_kb`, `gv_hints`, `gv_footer` (374,1141,1147,2004) | C; own keycaps/text layout | no/no/— | HINT; player footer |
| G7 | Grid/card/catalog/title and sports card surfaces: `gv_row`, `gv_cards`, `catalog`, `title_screen`, `gv_league_card`, `gv_match_card` | C; raw rectangles, fixed active treatment; isolated PILL calls | no/no/no | ROW for schedule rows; MATERIAL + POSTER for artwork; MOTION; Home/Detail |
| G8 | Wizard choice cards, parental/PIN/kids/multiview controls, empty/error panels: `wizard`, `parental_screen`, `kids_screen`, `multiview_screen`, `gv_vazio`, `gv_erros_sheet` | C; custom surfaces; some generic `button` reuse of G3 | no/no/no | MATERIAL/ROW/PILL/TEXT; CLOCK for modal; player/Settings. Keep unique multiview video layout. |
| G9 | Fonts, icons, decoded art: `label`, `gv_label`, `gfx_icone`, texture helpers | R; TEXT/gfx primitives | no/no/— | Keep; style selection is from shared enum, not literal font sizes. |

Subview coverage: grade, cartoes, preview/preview-grade/preview-foco/preview-hero-cheio/preview-troca, dock, catchup, categorias, busca, numero, opcoes, canal-opcoes, ajustes, pastas, epg-* including health, erros/fontes-erro, vazio, esportes-*; setup type/credentials/test/import/categories/EPG/ready/file/phone; source management; VOD movies/series/title/episodes; parental PIN/blocked/unlock/list/settings/child-profile; multiview. All use G1–G9 through `g5ui_draw`/`gv_screen`. `go:189` swaps screen immediately; `gv_tick:2181` only handles channel-number timeout, not animations.

### Servers wizard (W1–W6)

| ID | Element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| W1 | Provider/credential/result panels: `srvsetup.c:519 panel`, `563 srvsetup_desenhar` | C; direct `gfx_vidro_folha`; own selected wash | no/no/no | MATERIAL + ROW; player islands |
| W2 | Actions: `srvsetup.c:528 button` | C; glass focus directly, raw idle rect | no/no/no | PILL + MOTION; Detail buttons |
| W3 | Server/library/manage rows: `srvsetup.c:535 row` and draw branches | C; calls own panel | no/no/no | ROW + MOTION; player rows |
| W4 | Key hints: `srvsetup.c:541 dicaChip` | C; own keycap | no/no/— | HINT; Sources footer |
| W5 | Step navigation/selected labels: `srvsetup_desenhar:610` | C; manually composed step selectors | no/no/no | SEG for navigable choice; PROGRESS for completion; player tabs/progress |
| W6 | Text, provider logos, QR: `text`, `paragraph`, `logo`, `drawQR` | R; TEXT/gfx/QR primitives | no/no/— | Keep. White QR backing is required contrast, not theme regression. |

Covers choose/manage/discovery/address/password/code/Quick Connect/Emby Connect/server lists/libraries/ready/remove. `opened`/`selected` control immediate visibility; `srvsetup_atualizar:422` contains no local visual interpolation. `servidores.c` itself has no draw function.

### Settings UX and tour

| ID | Screen / element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| S1 | v4 full shell: `ajustes_ux_v4_desenho.inc:97 aj4Pagina` | C; raw miolo/solid surface and heavy extra veil | no/no/— | SETTINGS `ajIlha` / MATERIAL; baseline Settings islands |
| S2 | v4 row surface: `:62 aj4LinhaDesenhar` | C; boolean `ajNeutro` | no/no/no | SETTINGS `ajFocoLinha` + MOTION; baseline Settings row |
| S3 | v4 action pills and Simple/Advanced/group selection: `:45 aj4Botao`, `:159 aj4Home` | C; wrapper owns style/selection composition | no/no/no | PILL + SEG + MOTION; player tabs / Settings actions |
| S4 | v4 footer text and headers: `aj4Pagina:128`, `aj4Lateral` | C; literal key-hint string, multiple `TXT_G30*`/`TXT_V2_*` choices | no/no/— | HINT + KICK/TEXT; Settings footer/header |
| S5 | v4 shared primitive wrappers: `aj4Neutro`, `aj4Pilula`, `aj4Icone`, `aj4Texto` | R; actual baseline `ajNeutro`, `ajPilulaAcento`, text/icons | no/no/no | Retain RTL coordinate wrappers, feed interpolated focus. |
| S6 | v4 preview/editor/profile forms: `aj4Amostras`, `aj4Editor`, `pfDesenhar` | R; baseline SETTINGS, preview scene renderers and text | partial/partial/no | Retain scene content; connect v4 transitions. |
| T1 | Tour card surface: `ajustes_ux_tour.inc:254 tourCartao` | C; white whole-card inversion | no/no/no | MATERIAL + ROW + MOTION; Detail cards |
| T2 | Tour two action pills: `:247 tourBotao` | C; hardcoded gray/white fills/ink | no/no/no | PILL + MOTION; player/Detail buttons |
| T3 | Tour NEW badge and header sparkles tile: `tourCartao:262`, `tourDesenhar:291` | C; fixed red / white tiles | no/no/— | PILL idle surface + dynamic status tint; TEXT support label; Sources chips |
| T4 | Tour kicker/title/support/body styles: `tourCartao`, `tourDesenhar` | C; own assortment, fixed contrast tied to T1 | no/no/— | KICK/TEXT; Settings header/row hierarchy |
| T5 | Tour header island, hints and live preview scene: `tourDesenhar`, `tourPrevia` | R; `ajIlha`, `ajDicasLinha`, `ajc*` renderers | no/no/— | Keep scene selection and real previews. |
| T6 | Tour scroll: `tourAtualizar:123` | R; `anim_mola2` | —/—/partial | Keep spring; add entry/exit/focus policy. |

v4 routes: home/tasks, section/groups, editor, search, presets, differences, help and profile. `ajustes.c:11168` selects tour, otherwise `aj4Desenhar`; legacy `paginaA` is updated at `10472` but not consumed by the v4 draw tree. Do not claim legacy animation animates v4. Neither source uses invented font files; issue is hierarchy/usage, not unregistered fonts.

### Começar

| ID | Element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| C1 | Name/import/tracker/ready option row/card surfaces: `comecar.c:357 superficie`, step0–3 | C; whole-row accent, own neutral/ink | partial/partial/no | MATERIAL + ROW; PILL only on action; Settings/player rows |
| C2 | Icon/initial discs: `:374 bola` | C; raw tile wash | partial/partial/no | MATERIAL for tile + PILL for action; player icon buttons |
| C3 | Step bars/header: `:406 cabecalho` | C; hand-composed bars and mixed typography | partial/partial/— | PROGRESS `plrui_trilho`, KICK/TEXT; player progress/header. No baseline four-step wizard exists. |
| C4 | Color choice selection ring: `desenha_passo0:447` | C; local 3px ring and binary growth | partial/partial/no | PILL/disc focus with MOTION; preserve actual swatch color as data; player discs |
| C5 | Actual avatar/color content and text: `avatar`, `titulo`, `txt_em` | R; baseline gfx/text primitives | partial/partial/— | Keep per-profile colors, use shared text styles. |
| C6 | Whole-page fade: `comecar_atualizar:330`, `comecar_desenhar:639` | R; `anim_suave`, baseline ramp wrapped by new `t21_entrada` | yes/yes/no | Keep reduced-motion handling; add step/focus interpolation. |

### Sources variants, filters, add-on/debrid sheets

| ID | Screen / element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| A1 | AIO Sources shell: `streams.c:6073–6099 stream_folha_desenharCorpo_` | C; bypasses `ilha_corpo`, custom art/title/slide composition | partial/partial/— | CLOCK + MATERIAL; regular Sources. Preserve wider required content through requested body dimensions. |
| A2 | AIO provider tabs: `streams.c:5770 corpoAio` | C; fixed gray selected rectangles | partial/partial/no | SEG + MOTION; player tabs |
| A3 | AIO source rows and group/header/footer typography: `corpoAio:5780–5808` | C; raw focus rect `.19/.205/.22`, index window jumps, no regular zero-results body | partial/partial/no | SOURCE `corpoFolha`/`focoFonte` + ROW/KICK/HINT, spring scroll; regular Sources |
| A4 | AIO Sources action chips/MP4 state: `corpoAio`, `chipFolha:5655` | R; retained baseline `chipFolha` | partial/partial/no | Keep chip semantics; baseline binary chip focus is inherited. |
| A5 | AIO Sources fonts/icons/badges | R; TEXT/gfx | partial/partial/— | Keep supported metadata. |
| A6 | AIO Sources page slide: `stream_folha_atualizar:5320` | R; baseline `anim_mola` | yes/yes/partial | Keep, integrate CLOCK without double transform. Player branch already uses `plrilha_pedir`. |
| F1 | Filter shell/header: `ajustes_ux_ilha.inc:2006 desenhaFolhaFiltros` | R; `ajVeuModal`, `ajIlha`, `ajCabecalhoFolha` | no/no/— | Baseline retained. |
| F2 | Filter category/value rows | R; `ajFocoLinha`, TXT_AJ/ILHA styles | no/no/no | Retain; interpolate focus. |
| F3 | Filter checkboxes | R; `ajPilulaAcento`, `ajNeutro`, `ajIconeT` | no/no/no | Retain exclusion semantics. |
| F4 | Filter close/hints/error text | R; `ajNeutro`, `ajDicasLinha`, text | no/no/no | Baseline retained. |
| F5 | Filter whole-sheet lifecycle/category replacement | C; binary `frAberta`, `ffCategoria`, direct render | no/no/no | CLOCK or baseline modal `mov_mola_assenta` + MOTION; clock/player sheet. Inherited implementation debt. |
| D1 | Add-on guide and Debrid breadcrumb kicker: `addonguia.c:146 kicker` | C; manual uppercase/tracking/text scale | yes/yes/— | KICK; regular Sources/player header |
| D2 | Guide/debrid/failure outer sheet: `painel:122`, `desenhar_:723` | R; new `t21_ilha_folha` wrapper plus baseline clock-centered `plrilha_pedir` | yes/yes/— | Keep centered island lifecycle. |
| D3 | Actions/service buttons: `botaoEm:131` | R; `botoes.c:botao_pilula` | yes/yes/yes | Keep. |
| D4 | Mode/add-on/provider rows: `rotas`, `modoDesenhar`, list draw | R; `plrui_linha_foco` | yes/yes/yes | Keep. |
| D5 | Capability/status chips: `chip:153` | R; `plrui_botao_repouso` | yes/yes/— | Keep compact informational badges; not all are actions requiring 56px. |
| D6 | Debrid connection/preparing content: `corpoDesenhar:562`, preparing/failure branches | R; `plrui_material`, shared text/icons, phone renderer | yes/yes/yes | Keep local-network/offline/preparing branches. |
| D7 | Scroll/focus/page reveal: `movimento:49`, `entradaDe`, `relogioMovimento` | R; baseline `anim_mola`, `anim_mola2`, reveal primitives | yes/yes/yes | Keep reduced-motion path. |
| D8 | Loading/progress tracks and QR | C; progress uses two raw `gfx_cor` bars (`corpoDesenhar:670–672`); QR primitives reused | yes/yes/— | PROGRESS `plrui_trilho`; player progress. Keep shared QR and unique diagrams. |

D1–D8 independently classify both Add-on guide and Debrid sheets, hence 2/8 each. AIO management workspace is a separate page below. Source filters F1–F5 are actual `desenhaFolhaFiltros`, not AIO's read-only filters inspector.

### Achievements

| ID | Element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| Q1 | Hub/retomar/map/family card bases: `conquistas.c:465 superficie`, `painel` | C; always-solid `.071/.075/.086`, own focus wash | yes/yes/yes | MATERIAL + ROW; player/Detail cards; honor glass setting |
| Q2 | Card tint overlay: `desenha_card:589` | C; achievement-colored focus rect | yes/yes/yes | ROW; keep earned badge color on emblem/progress, not generic row focus; player rows |
| Q3 | Action buttons | R; `pilula:474` calls `botao_pilula` | yes/yes/yes | Keep. |
| Q4 | Tabs/kickers/hints | R; `plrui_seg`, `plrui_kicker`, `plrui_dicas` | yes/yes/yes | Keep. |
| Q5 | Progress/level marks | R; `plrui_trilho` | yes/yes/— | Keep semantic achievement tint. |
| Q6 | Detail modal and notice | R; `plrui_material` + shared text/buttons; notice ramp | yes/yes/yes | Keep manual centered modal spring; CLOCK not mandatory for every central modal. |
| Q7 | Focus/scroll/modal/reveal | R; `anim_mola`, `anim_mola2_reduzida`, `mov_mola_assenta`, `anim_atraso` | yes/yes/yes | Keep. |

### Person History / Filmography

History means `filmo.c` História tab, not playback-history storage. Filmography, About, Conquests and overlays share renderer. Playback-watch status appears as content; no separate new History drawing module found.

| ID | Element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| H1 | History chapter chip layout/state: `filmo.c:142 chip`, chapter branch in `historia:391` | C; generic button surface is shared, selected chapter composition local | yes/yes/partial | SEG for exclusive chapter selection + MOTION; player segmented control |
| H2 | History hero/portrait/text | R; gfx/text primitives, portrait palette | yes/yes/— | Keep unique person presentation. |
| H3 | History action pills/year controls | R; `plrui_botao_repouso`, `plrui_pilula_foco`, `plrui_trilho` | yes/yes/yes | Keep. |
| H4 | History tabs/kickers/hints | R; `plrui_seg_txt` (new wrapper over baseline SEG), KICK/HINT | yes/yes/yes | Keep underlying component; no claim wrapper existed in 2.0.8. |
| H5 | History poster row | R; `foco_zoom`, `home_vitrine_cartao`, badges | yes/yes/yes | Ring duplication counted under Filmography P1 only, shared implementation. |
| H6 | History motion/state | R; `filmo_atualizar:67`, `sobe`, `t21_estado` wraps baseline text/material | yes/yes/partial | Keep springs; retain outgoing tab body during replacement. |
| P1 | Filmography optional poster ring: `filmo.c:223 cartaz` | C; copied `foco_anel` implementation | yes/yes/yes | POSTER `foco_anel`; Home poster rows; preserve optional border |
| P2 | Grid artwork and badge | R; `home_vitrine_cartao`, `badges_check_cartao`, `foco_zoom` | yes/yes/yes | Keep. |
| P3 | Filters/years/actions | R; material/pill/progress helpers | yes/yes/yes | Keep. |
| P4 | Tabs/kicker/hints | R; shared SEG/KICK/HINT | yes/yes/yes | Keep. |
| P5 | About/Conquests panels | R; `material:115` calls `plrui_material`; TEXT | yes/yes/— | Keep; verify fixed About columns at scale extremes. |
| P6 | Title options/quiz/date/decline/biography sheets: `overlay:423`, `corpoFilmo:445` | R; `ilha_corpo` retains previous body, shared pills | yes/yes/yes | Keep clock callbacks and pointer transform. |
| P7 | Scroll/focus/loading/empty states | R; springs, reveal, `t21_estado` | yes/yes/yes | Keep. |

### Library / Origens and Lists

Drawing lives in `biblioteca.c`; `listas.c` supplies data, not an independent canvas. Lists includes fixed/mine/public/collections, list detail/history/notes, trails and share.

| ID | Screen / element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| B1 | Origens letter/status discs and hand-tracked heading: `biblioteca.c:3317 desenhaOrigens` | C; per-origin raw disc/text composition | yes/yes/yes | MATERIAL/PILL for action disc; KICK for heading; player/Settings. Keep provider logos and identity letters. |
| B2 | Library tabs: `desenhaModos:2188` | R; `plrui_seg_ex` extends baseline `plrui_seg` | yes/yes/yes | Keep. |
| B3 | Title posters/list/timeline rows: `desenhaCartaz`, `desenhaLinhaTitulo`, `desenhaWatched` | R; baseline retained drawing + `home_vitrine_cartao`, `plrui_linha_foco` | yes/yes/yes | Keep. |
| B4 | Origens panel/row focus | R; `t21_ilha` dispatches shared material; `plrui_linha_foco` | yes/yes/yes | Keep. |
| B5 | Library filter/action chips: `pilula:2019`, picker/actions | R; retained baseline `pilula`, glass/focus primitives | yes/yes/yes | Do not label inherited drawing a new regression. |
| B6 | Header/text/badges/thumbnail fallback | R; KICK/TEXT, baseline gfx and title card | yes/yes/— | Keep. |
| B7 | Loading/error/empty panel | R; `desenhaVazio:2463`, `t21_estado` composition of baseline material/text | yes/yes/yes | Keep retry focus and data states. |
| B8 | Library motion | R; `biblioteca_atualizar:1825`, focus/scroll/reveal springs | yes/yes/yes | Keep safe origin from `t21_inicio`. |
| L1 | List grid source badge: `desenhaCartaoLista:3085` | C; raw black badge + `TXT_MINI` | yes/yes/— | PILL `plrui_botao_repouso` + TEXT `TXT_ILHA_HORA`; Sources capability badge; preserve only useful provenance |
| L2 | List tabs/filters | R; SEG extension and retained baseline pill | yes/yes/yes | Keep. |
| L3 | List grid card base/focus/poster mosaic | R; `plrui_material_s`, `plrui_linha_foco`, gfx artwork | yes/yes/yes | Keep. |
| L4 | Mine/public rows | R; `desenhaLinhaMinha:3173`, `desenhaLinhaLista:3277`, baseline `superficieLinha` now shared ROW | yes/yes/yes | Keep. |
| L5 | List info/history/curator panels | R; `desenhaPainelLista`, `desenhaPaginaCab`, `t21_ilha` / KICK | yes/yes/— | Keep content and attribution. |
| L6 | Trail steps/notes/about | R; `desenhaTrilha:3790`, ROW/material/text | yes/yes/yes | Keep unique ordered-trail content. |
| L7 | Share overlay | C; `desenhaCompartilhar:3754` uses full-screen .82 veil, raw heading/URL and QR; binary `compartilha` | no/no/— | CLOCK + MATERIAL + TEXT/HINT; Sources clock sheet. Keep QR bitmap and URL. |
| L8 | List states/motion | R; Library springs/reveal and loading/empty handling | yes/yes/yes | Keep. |

### AIO dashboard

| ID | Element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| I1 | Setup step dots, status/config/ready badges and hand-built clock: `aiostreamsui.c:580`, `594`, `601`, `658`, `workspace:792–812`, `desenhar:1111` | C; fixed teal/amber fills and standalone clock | yes/yes/— | PROGRESS for activity/steps, PILL idle/state badge + dynamic accent, CLOCK for existing app clock; player/Sources |
| I2 | Setup/workspace/inspector panels: `painel:474` | R; `t21_ilha_folha`, baseline glass primitives | yes/yes/— | Keep; wrapper is new, primitives retained. |
| I3 | Sidebar and catalog/service/link/subtitle/filter/health rows: `linha:516`, `workspace:780` | R; `plrui_linha_foco`, `plrui_material` | yes/yes/yes | Keep. |
| I4 | Buttons: `botaoIc:533`, `botaoW` | R; `botao_pilula` | yes/yes/yes | Keep. |
| I5 | Header/kicker/footer | R; TEXT/KICK/HINT | yes/yes/— | Keep. |
| I6 | Busy/manifest-only/error and phone/QR | R; `plrui_anel`, shared phone renderer, TEXT | yes/yes/— | Keep. |
| I7 | Presets/rules/title inspectors | R; same panels/rows/buttons; unique rule text | yes/yes/yes | Keep read-only/version/CAS semantics. |
| I8 | Motion: `movimento:58`, `itemInicio`, `desenhar:1102` | R; baseline ramp/spring/reveal primitives | yes/yes/yes | Keep outgoing-page fade state, spring focus/scroll. |

### Detail / Juntos / Servers / Social / Home

| ID | Screen / element / current code | Class / dependency | O/C/F | Replacement / baseline |
|---|---|---|---|---|
| E1 | Detail ratings summary/source/own/friends tiles: `detail.c:1326 desenhaMediaTile`, `1370 desenhaNotasTiles` | C; raw tile fill, own-note ring, duplicated comparison/progress bars | yes/yes/yes | MATERIAL `plrui_cartao` + ROW; PROGRESS `plrui_trilho`; TEXT. Baseline Detail cast/trailer/More cards. Keep semantic critic/public data colors if necessary, no decorative focus color. |
| E2 | Detail hero and normal/server actions: `heroWeb:5092`, `desenhaBotao`, server action branches | R; retained baseline draw, glass/pill helpers | yes/yes/yes | Keep personal and server behavior. |
| E3 | Season/episode/shuffle/watch badges | R; retained `desenhaTemporada`, `desenhaEpisodio`, shared focus and badges | yes/yes/yes | Keep B2 spacing and watched badge. |
| E4 | Cast/trailers/studios/related | R; retained `desenhaElenco`, `desenhaTrailer`, shared gfx/focus | yes/yes/yes | Keep. |
| E5 | Collections/trails/timeline/music content | R; retained card primitives, ROW, shared buttons/material | yes/yes/yes | Keep content-specific composition. |
| E6 | More / synopsis / rating / episode menus | R; baseline `detmais`, `sinopse`, `desenhaNota`, menu helpers | yes/yes/yes | Keep baseline lifecycle; no blanket modal rewrite. |
| E7 | Full comment overlay and friends sheet | R; MATERIAL; `amtui_desenhar` ROW | partial/partial/partial | Retained modal debt; isolate from new ratings replacement. |
| E8 | Section headers/text/loading skeletons | R; baseline text/gfx and reveal | yes/yes/— | Keep. |
| E9 | Detail transitions/focus/scroll | R; `detail.c:4301–4467` baseline springs, section stagger | yes/yes/yes | Keep. |
| J1 | Lobby seat/avatar surfaces: `juntosui.c:751 desenharLugar` | R; `t21_ilha_solida` using baseline gfx; ROW and PILL; shared avatar | yes/yes/yes | Intentional solid seats over cinema art; do not globally convert to translucent. |
| J2 | Lobby actions: `desenharSala:922` | R; `botao_pilula` | yes/yes/yes | Keep. |
| J3 | Lobby text/status/QR | R; TEXT/shared icons/QR | yes/yes/— | Keep actual session/member counts. |
| J4 | Invite/code/source/reaction/leave/admission sheets | R; `pedirCorpo:1185`, CLOCK; rows/buttons shared | yes/yes/yes | Keep retained closing callback. |
| J5 | Source/member rows | R; ROW/PILL and shared metadata | yes/yes/yes | Keep. |
| J6 | Notices/countdown/reaction motion | R; island notices and baseline anim/text primitives | yes/yes/— | Keep unique reaction content. |
| J7 | Focus/scroll/layer movement | R; `juntosui.c:554–570`, `desenharCorpo`, `entrada` | yes/yes/yes | Keep. |
| V1 | Server Library pages | R; `servidores_bibliotecas` data into B2–B8 | yes/yes/yes | Keep. |
| V2 | Server title hero/actions | R; `detail.c:heroServidor`, E2 | yes/yes/yes | Keep. |
| V3 | Server title source rows | R; `servidores_fontes_colher`, baseline Sources renderer | yes/yes/yes | Keep. |
| V4 | Server seasons/episodes | R; E3 | yes/yes/yes | Keep. |
| V5 | Server art/brand metadata | R; baseline gfx/texture/TEXT | yes/yes/— | Keep provider identity. |
| V6 | Server loading/empty/error presentation | R; Library/Detail state branches | yes/yes/— | Keep. |
| V7 | Server focus/scroll/transitions | R; host Library/Detail/Source springs | yes/yes/yes | Keep. |
| N1 | Active friend profile: `amigoperfil.c:241 amigoperfil_desenhar` | R; baseline portrait/text/poster grid renderer | yes/no/yes | Keep. `app.c:7279` routes Social here. |
| N2 | Friend numbers/empty rows | R; baseline `numero`, `vazioFila` retained | yes/no/— | Keep; safe-area/runtime scale check only. |
| N3 | Friends management: `pessoas.c:1367 pessoas_desenhar` | R; MATERIAL/ROW/PILL/HINT, loading ring | yes/yes/yes | Keep. |
| N4 | Home Friends row: `amigosfil.c:369` | R; baseline `svd_rosto`, focus treatment | yes/yes/yes | Keep normal focus; do not add See all/Add actions. |
| N5 | Title Friends sheet: `amigostitulo_ui.c:90 amtui_desenhar` | R; MATERIAL/ROW, baseline exponential entry/scroll | yes/no/no | Baseline debt; no new regression. |
| N6 | Legacy activity page: `social.c:187 social_desenhar` | R; shared avatar/poster/ROW and new state composition | yes/yes/yes | Added `anim_mola`/reveal at 177 improves baseline. Not active TELA_SOCIAL route. |
| N7 | Social avatars/posters/chips | R; `svdesenho.c:svd_*` baseline functions | yes/partial/yes | Keep. |
| N8 | Social states/hints | R; baseline text/shared state/material/ring/hints | partial/partial/— | Keep private/disconnected/stale semantics. |
| O1 | Home hero/buttons | R; `desenhaHero`, baseline hero pill/button draw | yes/yes/yes | Keep. |
| O2 | Home poster/wide/Cover Flow/Stage/large/round cards | R; retained `home.c` renderers, GFX_CARD/focus helpers | yes/yes/yes | Keep each approved layout. |
| O3 | Home headers/rank numerals | R; `desenhaTituloFil`, `desenhaNumeral` retained | yes/yes/— | Keep. |
| O4 | Home progress/Continue and title chips | R; `desenhaFaixaAberta`, retained progress/metadata | yes/yes/yes | Keep. |
| O5 | Friends state chips | R; `home.c:7341 desenhaChipAmigos` exists in baseline | yes/yes/— | Keep; not a new duplicate. |
| O6 | Missing artwork/skeletons | R; `desenhaPlaceholderHero`, `desenhaArteAusente`, `desenhaCarregando` | yes/yes/— | Keep. |
| O7 | Empty first-use/QR | R; `homevazio.c` shared QR / baseline text | yes/yes/yes | Keep. |
| O8 | Home spring/reveal/Detail handoff | R; `revela_entra`, focus sweep, scroll springs, `detail_transicao` | yes/yes/yes | Keep. |

Unique content excluded from custom numerator: Juntos cinema illustration/reaction glyphs; achievement trophy/emblem artwork; person portrait palette/story timeline data; preview scenes inside Settings/tour; provider logos; QR bitmap/white contrast backing; EPG time geometry and multiview video panes. These have no exact 2.0.8 semantic counterpart. Replace their duplicated surrounding control surfaces, not the content.

## Findings that must not turn into mistaken fix jobs

| Concern | Evidence / boundary |
|---|---|
| Glass wrapper provenance | `tela21.h` is new. `t21_ilha` delegates to `plrui_material` in glass; solid branch uses baseline gfx primitives. `t21_ilha_folha` uses raw shared glass at different opacity. Count retained material primitive as reuse, not exact opacity parity; compare glass/solid in fix jobs. |
| Poster rings | Optional configured border is supported in 2.0.8 (`foco_anel`). Do not delete that preference because DESIGN's default has no ring. Filmography should call helper, not duplicate it. |
| Hardcoded colors | Fixed neutral/background, QR contrast, provider identity, artwork and earned badge colors are not automatically regressions. Fixed interactive focus/selected fills are actionable. |
| Exact old semantic component | No 2.0.8 achievement hub, four-step Começar, IPTV wizard or chapter timeline. Replacement catalog names real primitives only; preserve requested functionality/composition. |
| Static-only conclusions | No proof of clipping, frame pacing, runtime accessibility, pointer alignment at all scales, or pixel parity. M flags require captures/interaction in subsequent jobs. |
| History ambiguity | Audited person História + Filmografia and all their overlays. If central intended separate diary/history UI, identify its production entry point before scheduling an extra screen job. |

## Verification

| Command from worktree root | rc | Result |
|---|---:|---|
| `bash tests/servidores_material.sh` | 0 | Shared rimless material check passed; restores rim state. Does not validate wizard appearance. |
| `bash tools/build-local.sh` | 0 | Local build passed; existing `ATOMIC_VAR_INIT` deprecation warning in `src/perfis.c:73`. |

No PNGs, device deployment, push or UI modifications. Report copies committed under `docs/reviews/essencia/`; requested external paths contain identical text. Fix jobs in `plano.md`; no implementation performed.
