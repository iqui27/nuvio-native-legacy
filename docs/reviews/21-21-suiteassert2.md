# 21-21-suiteassert2: ASSERT set, second half

Source list: `docs/reviews/21-suite-novas.md` (branch agente/21-21-suiteverde).
Each test was rerun first, 6 at a time, on a disk with 33 GB free. Classes: NOISE = passes now (disk-full or load in the original run); APP = app regression, fixed in src; STALE = assertion or harness out of date after an approved change; ENV = needs something outside the repo; OPEN = not fixed here.

| Test | Class | Cause | Fix | Commit |
|---|---|---|---|---|
| listastrava_checks | NOISE | passes on rerun | none | - |
| plx_legenda_prefetch | NOISE | passes on rerun ("Directory not empty" = disk full) | none | - |
| tex_memory | NOISE | passes on rerun (mkdtemp, disk full) | none | - |
| text_largura | NOISE | passes on rerun (mkdtemp) | none | - |
| trakt_cine_neg | NOISE | passes on rerun | none | - |
| vaitocar | NOISE | passes on rerun (mkdtemp) | none | - |
| veutrans_check | NOISE | passes on rerun | none | - |
| vidro_orcamento | NOISE | passes on rerun (mkdtemp) | none | - |
| vidro_qualidade | NOISE | passes on rerun (mkdtemp) | none | - |
| webos_network_builds | NOISE | passes on rerun | none | - |
| versao_208 | STALE | default version still 2.0.8; 2.1.0 bump came in ec5d46bee | default to 2.1.0 | a4784d255 |
| setuptv, plugins_fila | APP | `tools/idiomas.py`: 40 empty Arabic strings (AIOStreams/Trakt/manifest strings), 1 Arabic left in English ("Manage fonts"), 23 Serbian-Latin strings written in Cyrillic | translated the Arabic; transliterated the Serbian values to Latin (the "BG · България" endonym stays native) | e83857c61 |
| tela21 | APP + STALE, OPEN | (1) Onboarding keeps the mock's 120 px margin (97b2fcf8a) and so does the Filmography History tab (db0f8404a); (2) filmo.c's `#undef ponteiro_alvo` wrapper (d34e0a548) removed the harness instrumentation; (3) pills now draw through `botao_pilula_sem_aro` (555f019fe); (4) History focus now goes tabs → chapters → years → posters; (5) APP: d34e0a548 dropped "Back from content goes to the tab row first" in Filmography, which filmografia_shot also expects | (5) restored that one clause in src/filmo.c; (1)–(4) test updated, citing each commit | 95abc4d91, 08440b658 |
| | | still failing: the options sheet now draws inside the island body (`ilha_corpo`, drawn later by `ilha_desenhar` after the open animation), so the harness sees no sheet targets; also one zero-width paint rect at History 80% (218,555 w=0) | not done: needs a harness driver for the island animation | - |
| rewatch_quit | STALE | d80e00b15 put the remote-tips route between the QUIT and rewatch lines; the extraction took only one line above | extract from the QUIT line through the rewatch line; stub the new calls. The app is correct (QUIT is handled before the modal) | 9f2db6caf |
| video_juntos | STALE | (a) video sample stamps use `nv_ticks64` since a877c0f7c (old webOS SDL); the test bracketed them with SDL_GetTicks64. (b) juntos_cliente: playback hooks only run while the room owns the player (2b3d254b0). (c) tpk-escolha link: missing stubs for audio normalize and language prefs (0746d8de3) | same clock in both tests; fixture sets the room phase and also checks that outside a room nothing is forwarded; 4 stubs in the shared dts_tpk_stub.c | 210fe9361, 3180ca7be, 14d3c5260 |
| quickanim | STALE | one-time proof: "before" = `integracao/2.1`, which now has the fix (c0532919d) | pin "before" to c0532919d^ (still fails there for the expected reasons) | 7e3325628 |
| shufflepos, shuffleserie | STALE | the season row gained a spoiler-switch column (ce2fe50b6); sections now reveal on the wall clock (64ebda087), so the fake-clock frames had alpha ≤ 0.3 and no pointer targets | count + 2; wait 1.2 s of real time before the pointer check | fd4b10798 |
| spotlight_regressao | STALE | #708 (fd438a50b) sends Spotlight cards through the poster service | asserts the provider URL when a provider is on; with no provider, the result's poster, then its background | 0a01ee325 |
| streams_folha_cache | STALE | the filename marquee (af04cde38) calls `txt_linha_bruta` and `gfx_recorte_dentro`, which the headless harness did not stub → segfault without a GL context | headless stubs | d5ad341fa |
| mkvass | STALE | the fixture attached InterDisplay-Regular.ttf, which grew from 408 KB to 1.0 MB (legacy kerning, ee3f17266); the font alone broke the "< 2 % of bytes" budget | attach JetBrainsSans-Regular.ttf (120 KB); budget unchanged (now 0.67 %) | 6f4ab6b38 |
| i607 | STALE | the JS bridge was renamed `nv_http_iniciar_bruto` (26ddf22d7, C wrapper adds nv_media_url); the subtitle picker now calls legenda.c | bridge name updated; legenda.c and watchstate.c linked | a5b11360f |
| juntos_ux | STALE | the clock-island room card only shows outside the player (765368e4c); the test rejoined and expected the card without leaving the player | Back out of the player (`juntos_ao_voltar`), then reopen it for the host-source step | e9958b27e |
| nav21 | STALE + OPEN | Library fixture crash: the Origins panel can be collapsed and is saved per profile (1c7d6a7b3); the generic pass left it closed | reset `origensAbertas` in openFixture | 6396d6169 |
| | | now runs to the end: 3632 checks, 152 violations across 20 screens (Library 33, Lists 51, AIO 37, Settings/Debrid 12, Back-transition checks on History/Filmography/Detail). The test did not exist in 2.0.8 | not done: a broad navigation audit, needs its own job | - |
| xtvod_regression | OPEN (other branch) | guiav5_bugs segfaults | fixed by agente/21-21-suitecrash 25fc1f9cb; with that diff applied, all 8 sub-checks pass | - |
| ios_m3_builds | NOISE | hit the 420 s limit in the parallel suite | alone: ios_build, ios_bundle_seguro, build-local and android all rc 0 (206 s) | - |
| perfil_fundo_cobre | OPEN | the picker fix (8be255b35) is still in place. Bisect over first-parent exports: fill-cor 1.00 → 1.02 at 8814b2f8d (perfis21, owner-approved profile visuals); full-screen draws 2.0 → 2.2 at c83fb6fcf (embybib, `homevazio_ativo` reorder; reverting just that reorder gives 2.0). Neither change touches the picker's clear | not done: raising the thresholds would weaken the guard; the fixture state before the picker scene needs pinning | - |
| musica_visual | ENV | runs `/Volumes/ExternalSSD/tmp/musica-visual/render.cjs`, a scratch script that is not tracked in git and is now gone | not fixable in the repo; needs the script added to the tree | - |

Out of scope (separate branch): ajustes_tour, ajustes_ux_interacao, ajustes_ux_toque_modo*, ajustes_tri*.
No visual or layout change on any screen. The one app behavior fix (Filmography Back) and the i18n strings are not visual.
Possible overlap: the idiomas fix (setuptv/plugins_fila) is the same failure as biblioteca_menu_index in the first half (suiteassert1, no commits yet).
