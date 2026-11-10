# 21-21-suiteassert1: ASSERT failures, first half (release gate 2.1.0)

Source list: `docs/reviews/21-suite-novas.md` (branch agente/21-21-suiteverde), class ASSERT.
Base: integracao/2.1. Final rerun: 26/29 rc 0; `tools/build-local.sh` rc 0.
Each test was rerun first. Classes: ENV = noise from the disk filling up; STALE = assertion out of date after an approved change; APP = app regression; OPEN = not fixed, needs a decision.

| Test | Class | Cause | Fix | Commit |
|---|---|---|---|---|
| desktop-final | ENV | "No space left" / tools during the full run | none; passes on rerun | - |
| detail_rede_trava | ENV | timing under a full disk / load | none; passes on rerun | - |
| fontenome | ENV | mkdtemp, disk full | none; passes on rerun | - |
| fontes_quadro | ENV | mkdtemp, disk full | none; passes on rerun | - |
| fundo_dither | ENV | SaveBMP, disk full | none; passes on rerun | - |
| diagnostico | STALE | Android texture ceiling = auto budget (211efce22, 395189bf0) | Android policy checks its own table; the Quality-raise case moves to LG (same numbers) | 71555cee1 |
| diagnostico_persistencia | STALE | same | compiles as LG host; 192->300 (LG ceiling) | 71555cee1 |
| fundo_din_escuro | STALE | 3-scope settings (f40ae2c10): device option is now profile-local | asserts `escopoDe == ESC_PERFIL_LOCAL` | 962637725 |
| guiapolir (1st, 2nd assert) | STALE | preview audible by default (5b0491ed0); preview = owner's marked frame (123ac39a7) | toggle order flipped; the frame rect is checked for both sizes | 0dcf43589 |
| ajustes_iptv | STALE | rows live in topic submenus (502a111b5) | `tocar()` opens the owning topic first | 167a16adb |
| android_speed_room | STALE | stamp uses nv_ticks64 (a877c0f7c, webOS C9 crash fix) | stamps bracketed by the real monotonic clock | bf458a1dc |
| artetamanho | STALE | fetch backpressure / stopped-cache drop (211efce22) | direct fetches run with a live cache and a 128 MB budget | 4649f71be |
| busca_toque_modo | STALE | search calls celb_digitar_disponivel (85ac74587) | stub it too | 621dcf29b |
| biblioteca_menu_index | APP (i18n) | 41 empty Arabic strings, one wrong ("Manage fonts"); 24 Cyrillic lines in the Latin Serbian table | filled Arabic; transliterated Serbian to Latin; fixed 4 bad sr strings | e0e5d0c68 |
| central_rotulos | APP (i18n) | ru "Долби Видение" 123 px > 120 px | brand left untranslated (as in uk and most other tables) | 7853c596a |
| ctxmenu_contract | STALE | CTX_MAX split per build (6e712de79): script read two numbers | ceiling checked per build (TV 10, desktop 16); list picker counted against CTX_PICK_MAX | 93dcadc7f |
| desktop_settings_r13 | STALE | Palco list visible only after entering the category and topic (502a111b5) | enters category, then topics, before the pointer check | 7eb2aca47 |
| fonteescolha | STALE | every manual pick is exact (e6544a115) | wiring greps follow `abrirFonteManualDireta` and assert no fallback | c6d579b68 |
| expandebotao | APP | ordered expand of "Assistir do começo" (#205, 1fbf2ac25) lost in 2.1: label clipped while the width grows | restored anim_expande + unclipped label from 2.0.8; same sizes and layout | 2f0e9f6cc |
| desmarcarfrente | STALE | menu sends per item (e2d98f5f0) | capture visto_episodios_item | a32b5d7cc |
| detail_assistido | STALE | TV row Play/List/More; Watched is in More (397c75e4b) | helper opens More and picks Watched with real keys | 6670a6a82 |
| espaco | STALE | spatial keyboard nav (6bed5d341), 4-key command row (f1b3b5e67) | D-pad model reaches Space from column 0 | 91c51c6b3 |
| fonte_qualidade | STALE | MP4 DV profile 5 kept below same-band HDR10 (d4f1dca81, owner) | expectations follow; fonte_lg_mp4.c already did | b9398d1cb |
| gifperfis | STALE | Android <3 GB gets no GIF worker (211efce22); macOS has no /proc/meminfo | test decoder sees a 4 GB MemTotal | b5ed41190 |
| colecaoround | STALE | round focus ring = svd_aro_foco(disco,.5f), a GFX_COR circle (8633fccfa) | GFX_COR allowed only when the ring is on; script pins the full radius and no raw gfx_cor | 7caa48fbb |
| detail_ep_fonte_foco | STALE (crash) | sheet marquee calls gfx_recorte_dentro (af04cde38); headless test has no GL -> segfault | stub gfx_recorte_dentro/voltar | e0efc2979 |
| fundo_paleta | STALE | palette is an editor blade (bb31fa345); accent grid nav only in Palco (239b6fe6c) | blade path checks drawn Champagne pixels; accent section pinned to Palco layout | f64a93354 |
| ajustes_polish | OPEN | the Palco submenu draws no pointer Back target (`triPontSubVoltar` only in the non-blade header) | none: needs a visible Back control on an approved screen | - |
| guia_previa_grande | OPEN | 123ac39a7 makes the preview rect the marked frame regardless of "Tamanho da prévia"; the option no longer changes the rect | none: owner decides whether the option survives or the rect returns | - |
| guiapolir (compare) | OPEN | mv-switch-4/-8 mid-morph frames: stdev 0.035/0.045 < 0.05 nonblank bar (settled frames 0.06-0.07); no drawing change since fc92c0b91 | none: lowering the bar would weaken the check; needs deterministic morph capture or owner OK | - |

base-dono: no screen drawn. The only app visual change restores the 2.0.8 approved #205 behaviour.
