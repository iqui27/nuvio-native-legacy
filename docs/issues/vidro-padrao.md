# Vidro padrao - glass UI audit (branch agente/205-vidropadrao)

Owner: "tem varios componentes que nao estao seguindo o glass UI, tem que padronizar."

## Standard

| Token | Value | Where |
|---|---|---|
| Material | `gfx_vidro_material(r, raio, dens, a)` = `gfx_vidro(PAINEL, gray .06, dens * opacidade)` | gfx.c |
| Density | `GFX_VIDRO_DENS` .80 (panels, pills) / `GFX_VIDRO_DENS_MODAL` .86 (modals, cards) | gfx.h |
| Solid fallback | `gfx_vidro_ou_solido(..., sr,sg,sb,sa)`: glass under "Interface de vidro", else the old flat colour | gfx.h |
| Sheet | `gfx_vidro_folha` (gray .09, Ajustes opacity) for full side sheets | gfx.c |
| Radius / pad / margin | `NV_FOLHA_RAIO` 28 / `NV_FOLHA_PAD` 40 / `NV_FOLHA_MARGEM` 24; pills radius .5 | layout.h |
| Rim, shadow | `gfx_luz_canto` + short `GFX_SOMBRA`, no outline | plrui.c |
| Budget | one backdrop capture per frame; nested / over video / "Vidro fosco" off / minimal effects => flat scrim | tests/vidro_orcamento.sh |

Gate: `bash tests/vidro_padrao.sh` (FAIL before this branch: 30 files drew their own dark `gfx_cor` body; PASS after).
A dark translucent body (rgb .03-.09, alpha >= .5) drawn with `gfx_cor` fails unless it sits in an `else` (solid fallback) or carries `// vidro-ok: why`.

## Table (helper = draws through the shared glass helper)

| Component | file:line | helper | radius / pad / margin vs standard |
|---|---|---|---|
| plrui_material (27 callers: player overlays, sidebars, recenviar, streams, perfilsel, pausao, posplay, reacao, detmais...) | plrui.c:23 | yes (was own material) | caller radius, kept |
| Player disc OSD | plrui.c:60 | yes (new) | .5 pill |
| Ajustes island / miolo | ajustes_ux_desenho.inc:147,164 | yes (new) | 28 |
| Ajustes scene piece (preview over photo) | ajustes_ux_visual.inc:360 | yes (new) | per-call |
| Central de controle / notification & clock island panels | via ilha.c, ajIlhaMaterial | yes | 28 |
| Telemetria dialog | telemetria.c:144 | yes (new) | 28 |
| Atualizacao dialog | atualizacao.c:1529 | yes (new) | 28 |
| Keyboard grid (2 sites) | teclado.c:1277,1360 | yes (new) | 28 |
| Cel botao dialog | celbotao.c:258 | yes (new) | 28 |
| Novidades popups x11 | novidades.c:161, novidades11.c:814, 12:363, 13:171, 131:156, 132:120, 133:119, 134:118, 139:154, 180:805, 170:356/539/795 | yes (new) | 28 (180: 40) |
| Novidades cards / previews | novidades151.c:477, 160.c:653/684/1073, 170.c:1433, 201.c:702, 202.c:557, cartao.c:735 | yes (new) | 30-36, kept (layout-bound) |
| Novidades island pieces | dvtelaui.c:82, novidades201.c:220, 202.c:199, cartao.c:143 | yes (new) | per-call |
| Novidades pv cards | novidades201.c:479, 202.c:350, cartao.c:458 | yes (new) | PV_RAIO 28 |
| Novidades 20 side strip | novidades20.c:576 | yes (new) | 0 (edge strip) |
| Avisos toast + panel | avisos.c:963, 1748 | yes (new) | 28 |
| Agenda aviso | agendaviso.c:133 | yes (new) | 28 |
| Diagnostico card | diagnostico.c:2321 | yes (new) | 30 |
| Guia lembrete toast | guialembrete.c:151 | yes (new) | .5 pill |
| Guia hint pill | guia.c:1536 | yes (new) | .5 pill |
| Exit-back toast | home.c:7612 | yes (new) | 14 |
| Badge toast (check pill) | badges.c:487 | yes (new) | .5 pill |
| Friends pill | svdesenho.c:190 | yes (new) | .5 pill |
| "OK troca" hint pill | vertudo.c:2428 | yes (new) | pill |
| Intro cards (PiP, rec, recomenda, salvos) | pipintro.c:227, recintro.c:402, recomenda.c:4032, salvosintro.c:244 | yes (new) | .03-.035 of height |
| Phone card | telefonecartao.h:55 | yes (new) | 36 |
| Spotlight, ctx menu, agenda, salvos, explorar, amigos, menu, detail | spotlight.c, ctxmenu.c, agendaui.c, salvospainel.c, explorar.c, amigosfil.c, menu.c, detail.c | yes (already) | 28 |
| Streams / recenviar / guia sheets | streams.c, recenviar.c, guia.c | yes (already) | 28 |
| Faixas / legendas panels | faixas.c, legendasui.c | yes (via plrui_material) | caller |

## Documented exceptions (`vidro-ok`, not panels)

| Site | Why |
|---|---|
| agendaui.c:1953, ctxmenu.c:1299 | near-opaque base under the glass sheet (baked text bled through) |
| detail.c:7293, 7426 | full-screen scrims |
| novidades174.c:148, novidades201.c:710, 202.c:565 | backing/fade veil over preview art |
| svdesenho.c:28 | button fill |
| registro_ilha.inc:1368, 1518 | legibility backing over wallpaper, knob ring |
| registro_ilha.inc rgIlha | log panel kept opaque on purpose (Ajustes text ghosting, see comment) |
| ajustes_ux_desenho.inc:553, 632 | slider knob / disc |
| ajustes_ux_previa.inc, ajustes_ux_secoes.inc | Ajustes preview mocks |
| dicascontrole.c celular | illustration of a phone, not UI chrome |

## Not done

- Radius of Novidades cards (30-40) not forced to 28: art clipping and layout use the same constants.
- Padding/margin per component not re-flowed; only the material, density and rim path were unified.
