# Cloud settings recovery audit

## Baseline (before this change)

Requirement: every configured preference returns after logout/login or reinstall/login on the same TV. Hardware preferences must not leak to another TV.

(a) uploaded/restored; (b) restored only; (c) LOCAL without cloud backup; (d) outside ajustes.txt or action/status. No standalone (b) scalar found: existing web fields upload through the web merge, and c_settings_v1 uploads portable values even without a web counterpart. Platform-only rows included; dtsTpkLocal replaces dtsSaidaLocal on TPK.

| Option | Key | Baseline classification | Persistence path |
| --- | --- | --- | --- |
| `AJ_QUALIDADE` | `qualidade` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DV` | `dolbyVision` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ATMOS` | `dolbyAtmos` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LEG_LINGUA` | `legendaIdioma` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_AUD_LINGUA` | `audioIdioma` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PAUSA_OVERLAY` | `pauseOverlayEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_MANUAL` | `escolherFonteManual` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_AUTO` | `fonteAutoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_REPOR` | `fonteReporLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_TEXTO` | `fonteTextoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LANDSCAPE` | `modernLandscapePostersEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_CHEIO` | `modernHeroFullScreenBackdropEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_FUNDO` | `heroFundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_ARTE_DIF` | `heroDifferentFromCard` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_TRAILER` | `trailerHero` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FIL_LIMITE` | `-limiteFileiras` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FIL_ORDEM` | `-ordenarFileiras` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_RAIL` | `collapseSidebar` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RAIL_MODERNA` | `modernSidebar` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RAIL_BLUR` | `modernSidebarBlur` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO` | `heroSectionEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_CATALOGOS` | `-heroCatalogKeys` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_PS_FUNDO` | `perfilFundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DESCOBRIR` | `discoverLocation` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ROTULOS` | `posterLabelsEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NOME_ADDON` | `catalogAddonNameEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SUFIXO_TIPO` | `catalogTypeSuffixEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_OCULTAR_NLANC` | `hideUnreleasedContent` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NOTAS_HOME` | `homeImdbRatingsVisibility` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_GRAD_CLASSICO` | `classicFocusGradientEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_LIGADO` | `continueWatchingEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_OK` | `cwOkLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_FONTE` | `cwFonteLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_ESTILO` | `continueWatchingCardStyle` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_THUMB` | `useEpisodeThumbnailsInCw` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_BLUR_PROX` | `blurContinueWatchingNextUp` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_FURTHEST` | `nextUpFromFurthestEpisode` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_NAO_EXIBIDOS` | `showUnairedNextUp` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_ORDEM` | `continueWatchingSortMode` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_BLUR_NAO_VISTOS` | `blurUnwatchedEpisodes` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_TRAILER` | `detailPageTrailerButtonEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_META_EXT` | `preferExternalMetaAddonDetail` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_DATA_CHEIA` | `showFullReleaseDate` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_VEU` | `detalheVeu` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_TRAILER_AUTO` | `trailerAuto` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TRAILER_QUAL` | `trailerQualidade` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TRAILER_ASPECTO` | `trailerAspecto` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TRAILER_FONTE` | `trailerFonteLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_EXPANDIR` | `focusedPosterBackdropExpandEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_EXPANDIR_ATRASO` | `focusedPosterBackdropExpandDelaySeconds` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NAV_RAPIDA` | `fastHorizontalNavigationEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BORDA_FOCO` | `bordaFocoCartaz` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF` | `cardDepthEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_BORDA` | `cardDepthEdgeStrength` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_BRILHO` | `cardDepthSheenStrength` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_COBERTURA` | `cardDepthEdgeCoverage` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_POSTERS` | `cardDepthPostersEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_CW` | `cardDepthContinueWatchingEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_EPS` | `cardDepthEpisodeCardsEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_ELENCO` | `cardDepthCastEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROF_TRAILERS` | `cardDepthTrailersEnabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LARGURA_DP` | `posterCardWidthDp` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RAIO_DP` | `posterCardCornerRadiusDp` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_QUALIDADE_IMG` | `qualidadeImagem` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_IDIOMA` | `idioma` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ANIM` | `animacoes` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RESOLUCAO` | `resolucaoUi` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_TEMA` | `selected_theme` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_COR_LOGO` | `corDaLogoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PERFIL_ATIVO` | `-perfil` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SYNC` | `-sync` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ADDONS` | `-addons` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_STALKER_PORTAL` | `-stalkerPortal` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_STALKER_MAC` | `-stalkerMac` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_STALKER_LIMPAR` | `-stalkerLimpar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_XTREAM_SERVIDOR` | `-xtreamServidor` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_XTREAM_USUARIO` | `-xtreamUsuario` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_XTREAM_SENHA` | `-xtreamSenha` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_XTREAM_LIMPAR` | `-xtreamLimpar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_XTREAM_CONTA` | `-xtreamConta` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_EPG_PAIS` | `epgPaisLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SALVOS_DEST` | `salvosDestino` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TRAKT` | `-trakt` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SIMKL` | `-simkl` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SAIR` | `-sair` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_VERSAO_I` | `-versao` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ATUALIZAR` | `-atualizar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ENVIAR_LOG` | `-registro` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ENVIO_AUTO` | `envioAuto` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ESPACO` | `-espaco` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_TEX_MB` | `texturasMB` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_TMDB_LIGADO` | `tmdb_enabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_IDIOMA` | `tmdb_language` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_ARTE` | `tmdb_use_artwork` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_BASICO` | `tmdb_use_basic_info` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_FICHA` | `tmdb_use_details` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_DATAS` | `tmdb_use_release_dates` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_ELENCO` | `tmdb_use_credits` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_PROD` | `tmdb_use_productions` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_REDES` | `tmdb_use_networks` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_EPS` | `tmdb_use_episodes` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_TRAILERS` | `tmdb_use_trailers` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_MAIS` | `tmdb_use_more_like_this` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_COL` | `tmdb_use_collections` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TMDB_CW` | `tmdb_enrich_continue_watching` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_LIGADO` | `mdblist_enabled` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_CHAVE` | `-mdblistChave` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_MDB_TRAKT` | `mdblist_show_trakt` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_IMDB` | `mdblist_show_imdb` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_TMDB` | `mdblist_show_tmdb` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_LETTER` | `mdblist_show_letterboxd` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_TOMATES` | `mdblist_show_tomatoes` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_AUDIENCIA` | `mdblist_show_audience` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_META` | `mdblist_show_metacritic` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MDB_MAL` | `mdblist_show_mal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FANART_CHAVE` | `-fanartChave` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DIAGNOSTICO` | `-diagnostico` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_VELOCIDADE` | `-velocidade` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_MENU_EXPLORAR` | `menuExplorarLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MENU_GUIA` | `menuGuiaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MENU_AGENDA` | `menuAgendaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MENU_PERFIL` | `menuPerfilLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FOCO_TRAILER` | `trailerCartazModo` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ITENS_FILEIRA` | `itensFileiraLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_FONTE_UI` | `fonteInterface` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_GPU_EFEITOS` | `gpuEfeitosLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_VIDRO` | `vidroLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_P2P_LIGADO` | `p2pLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_P2P_URL` | `-p2pEndereco` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_P2P_TESTAR` | `-p2pTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_PROV` | `posterProvLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_POSTER_INST` | `-posterInst` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_TOKEN` | `-posterToken` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_EXTRA` | `-posterExtra` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_CHAVE` | `-posterChave` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_MODELO` | `-posterModelo` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_POSTER_TESTAR` | `-posterTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_HOME_LAYOUT` | `homeLayoutLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DEBRID_AD` | `-debridAD` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_AD_TESTAR` | `-debridADTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_RD` | `-debridRD` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_TB` | `-debridTB` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_PM` | `-debridPM` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_RD_TESTAR` | `-debridRDTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_TB_TESTAR` | `-debridTBTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DEBRID_PM_TESTAR` | `-debridPMTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_DET_SO_CINEMETA` | `soCinemetaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_IMDB` | `notaTituloImdb` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_TOMATES` | `notaTituloTomates` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_AUDIENCIA` | `notaTituloAudiencia` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_META` | `notaTituloMeta` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_METAUSER` | `notaTituloMetaUser` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_TRAKT` | `notaTituloTrakt` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_TMDB` | `notaTituloTmdb` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_LETTER` | `notaTituloLetter` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_MAL` | `notaTituloMal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_EBERT` | `notaTituloEbert` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_SCORE` | `notaTituloScore` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PERFIL_PESQ` | `-perfilPesquisavel` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_PERFIL_EDITAR` | `-perfilEditar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_VIDRO_CONTORNO` | `vidroContornoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_TRAILER_SOM` | `trailerDestaqueSomLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_TRAILER_ESPERA` | `trailerDestaqueEsperaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_CONCLUIDO` | `cwConcluidoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ADDONS_PRINCIPAL` | `addonsPrincipalLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_TRANSICAO` | `heroTransicaoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ADDON_POSTER` | `posterAddonLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ADDON_FUNDO` | `fundoAddonLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ADDON_LOGO` | `logoAddonLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_COL_ARTE_CONTA` | `colArteContaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SELOS_CORES` | `selosColoridosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_TRAILER_SOM` | `trailerDetalheSomLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LIVETV_RES` | `liveTvResolucaoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LIVETV_FORMATO` | `xtreamFormatoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LIVETV_ESPERA` | `liveTvEsperaLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LIVETV_DIAG` | `-liveTvDiag` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_LIVETV_MODO` | `liveTvModoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LIVETV_PROXY` | `liveTvProxyLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_RELOGIO` | `relogioTelaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RELOGIO_POS` | `relogioPosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SELO_VISTO` | `seloVistoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SEEKR_LIGADO` | `seekrLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SEEKR_CHAVE` | `-seekrChave` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SEEKR_TESTAR` | `-seekrTestar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SEEKR_FITA` | `seekrFitaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SEEKR_AJUSTE` | `seekrAjusteLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SAIDA_PLAYER` | `saidaPlayerLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_REACAO_CREDITOS` | `reacaoCreditosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PAUSA_COMENTARIOS` | `pausaComentariosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported (same as `reacaoCreditosLocal`) |
| `AJ_FONTE_PRAZO` | `fonteEsperaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_VER_REGISTRO` | `-verRegistro` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_MEDIDOR` | `medidorFormaLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_GUIA` | `-guiaUso` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SELOS_PACOTE` | `-selosPacote` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SELOS_PACOTE_ADD` | `-selosPacoteAdd` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_SELOS_PACOTE_REM` | `-selosPacoteRem` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_TAMANHO_UI` | `tamanhoUiLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_FUNDO` | `fundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_VIDRO_OPAC` | `vidroOpacLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_VIDRO_FOSCO` | `vidroFoscoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ICONE_APP` | `iconeAppLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_DISCORD` | `-discord` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_TAMANHO_AJUSTES` | `tamanhoAjustesLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LOGO_TRAILER` | `logoTrailerLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LEG_LINGUA2` | `legendaSecundariaIdioma` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG_SYNC_AUDIO` | `legendaSyncAudioLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_CACHE_SEEK` | `cacheSeekLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_TRAILER_ZOOM_TPK` | `trailerZoomTpkLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_PLUGINS` | `-plugins` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_JF_LIGADO` | `jellyfinLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_JF_SERVIDOR` | `-jellyfinServidor` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_JF_ENTRAR` | `-jellyfinEntrar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_JF_SAIR` | `-jellyfinSair` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_AVANCADAS` | `avancadasLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_POS` | `legenda2PosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_TAMANHO` | `legenda2EscalaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_COR` | `legenda2CorLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_FUNDO` | `legenda2FundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_BORDA` | `legenda2BordaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_EM_SERVIDOR` | `-embyServidor` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_EM_ENTRAR` | `-embyEntrar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_EM_SAIR` | `-embySair` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_PX_ENTRAR` | `-plexEntrar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_PX_SERVIDOR` | `-plexServidor` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_PX_SAIR` | `-plexSair` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FONTE_PRIORIDADE` | `fontePrioridadeLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_HDR` | `fonteHdrLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LOGO_APP` | `logoAppLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ABERTURA` | `aberturaAppLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ENQUETES` | `enquetesLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BUSCA_CINEMETA` | `buscaCinemetaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ESMAECER` | `esmaecerLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_BRILHO_PLAYER` | `brilhoPlayerLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_NOVIDADES20` | `-novidades20` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_MANTER_VIDEO` | `manterVideoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_DESCANSO_ESTILO` | `descansoEstiloLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DESCANSO_FONTE` | `descansoFonteLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_RELOGIO_12H` | `relogio12hLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TAM_MAX` | `tamanhoMaxLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TAM_MIN` | `tamanhoMinLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_APOIAR` | `-apoiar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_LEG_FORCADA` | `legendaForcadaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PLR_CLASSIF` | `classifPlayerLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_ESCOPO` | `streamAutoPlaySource` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_ADDONS_PERM` | `-fonteAddonsPermitidos` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FONTE_PLUGINS_PERM` | `-fontePluginsPermitidos` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FONTE_OUTROS` | `fonteOutrosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_REGEX` | `fonteRegexLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_REGEX_PADRAO` | `-fonteRegexPadrao` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FONTE_REGEX_MODELO` | `-fonteRegexModelo` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ESPACO_FILEIRAS` | `espacoFileirasLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ESPACO_TITULOS` | `espacoTitulosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROPORCAO_PADRAO` | `proporcaoPadraoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_CW_RETIDO_TAMBEM` | `cwRetidoTambemLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PS_CONTINUAR` | `psContinuarLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DTS_AC3` | `dtsSaidaLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_DV_MKV` | `dvMkvLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_HIST_CONTA` | `histContaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_SOCIAL` | `socialLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_ORDEM` | `-fonteOrdem` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FONTE_ORDEM_USO` | `fonteOrdemUsoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_TOCAR_CONFERINDO` | `fonteTocarConferindoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_AQUECER` | `fonteAquecerLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_CONFERIR_VARIAS` | `fonteConferirVariasLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_PREPARAR` | `fontePrepararLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_PROXIMO` | `cwProximoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BUSCA_NUVIO` | `buscaNuvioLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BUSCA_ORIGEM` | `buscaOrigemLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LAYOUT_AJUSTES` | `ajustesLayoutLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_P2P_LIMITE` | `p2pLimiteLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_P2P_DESTINO` | `p2pDestinoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_FONTE_ORDEM_ADDON` | `fonteOrdemLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG_SYNC_AUTO` | `legendaSyncAutoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG_MOSTRAR` | `legendaMostrarLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_AVISO_LEGAL` | `-avisoLegal` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_HDR_UI` | `hdrUiLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_TELA_ABERTURA` | `telaAberturaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CW_MINIMO` | `cwMinimoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BUFFER` | `bufferPlayerLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_PULAR_ABERTURA` | `pularAberturaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PULAR_RESUMO` | `pularResumoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROXIMO_AUTO` | `proximoAutoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PULAR_CREDITOS` | `pularCreditosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROXIMO_CONTAGEM` | `proximoContagemLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_INTERFACE_MODO` | `interfaceModoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_MIC_BUSCA` | `micBuscaLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_MENU_TOPO` | `menuTopoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ABAS_ESTILO` | `abasEstiloLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TECLADO_ANDROID` | `tecladoAndroidLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_LEG_CONTORNO_COR` | `legendaContornoCorLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FALHA_PROVEDORES` | `-falhaProvedores` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_PROVEDOR_ORDEM` | `-falhaProvedorOrdem` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_RD_LIMITE` | `-falhaRDLinks` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_TB_LIMITE` | `-falhaTBLinks` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_PM_LIMITE` | `-falhaPMLinks` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_AD_LIMITE` | `-falhaADLinks` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_ADDONS` | `-falhaAddons` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_ADDON_ORDEM` | `-falhaAddonOrdem` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FALHA_ADDON_LIMITE` | `-falhaAddonLinks` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_RETOMADA_MORNA` | `retomadaMornaLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_MANTER_PRAZO` | `manterVideoPrazoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ESTAT_TAMANHO` | `estatTamanhoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_OCULTAR_FILMES_VISTOS` | `hideWatchedMoviesLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ESTAT_LADO` | `estatLadoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_DICASCONTROLE` | `-dicasControle` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_HERO_GENERO` | `heroGeneroLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ABERTURA_ADDON` | `aberturaAddonLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_SINOPSE` | `heroSinopseLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_NOTAS` | `heroNotasLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_ANO` | `heroAnoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_DURACAO` | `heroDuracaoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_CURADORIA_URL` | `-curadoria` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_ABERTURA_FUNDO` | `aberturaFundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_HERO_SINOPSE_TAM` | `heroSynopsisSizeLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_EP_TAMANHO` | `epTamanhoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_PRECARREGAR` | `fontePrecarregarLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PROVEDORES_ESCOPO` | `-provedoresEscopo` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_MURAL_TITULOS` | `muralTitulosLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_AMIGOS_ESTILO` | `friendsRowStyleLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_IDIOMA_AUDIO` | `fonteIdiomaAudioLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_AUD_LINGUA2` | `audioSecundarioIdioma` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_BAIXAR_REGISTRO` | `-baixarRegistro` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FILTROS_FONTES` | `-filtrosFontes` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FILTRO_TAM_MAX` | `-filtroTamanhoMax` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_FILTRO_GRUPOS` | `-filtroGrupos` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_LEG1_TAMANHO` | `-legenda1Tamanho` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_FONTE` | `-legenda1Fonte` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_COR` | `-legenda1Cor` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_OPACIDADE` | `-legenda1Opacidade` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_FUNDO` | `-legenda1Fundo` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_POSICAO` | `-legenda1Posicao` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_BORDA` | `-legenda1Borda` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_ATRASO` | `-legenda1Atraso` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_NEGRITO` | `-legenda1Negrito` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_COR_BORDA` | `-legenda1CorBorda` | (d) outside ajustes.txt | player.txt; style already uploaded/restored in c_settings_v1 |
| `AJ_LEG1_RESTAURAR` | `-legenda1Restaurar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_LEG2_FONTE` | `legenda2FonteLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_OPACIDADE` | `legenda2OpacidadeLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_POSICAO` | `legenda2PosicaoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_ATRASO` | `legenda2AtrasoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_NEGRITO` | `legenda2NegritoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_COR_BORDA` | `legenda2CorBordaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LEG2_RESTAURAR` | `-legenda2Restaurar` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_COLECAO_EXIBICAO` | `colecaoExibicaoPadraoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PLAYER_EXTERNO` | `playerExternoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_TRAILER_AUDIO` | `trailerAudioLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PLAYER_ANIME` | `playerExternoAnimeLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_ABERTURA_ESTILO` | `aberturaEstiloLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_MENU_CARTAZ` | `menuCartazLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_TITULO_FIL` | `rowTitleStyleLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_VIDRO_QUALIDADE` | `vidroQualidadeLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_FUNDO_DIN` | `fundoDinamicaLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FUNDO_DIN_COR` | `fundoDinamicaCorLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ESCALA_TEXTO` | `escalaTextoLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_FONTE_GRUPO` | `fonteGrupoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FONTE_GRUPO_SESSOES` | `fonteGrupoSessoesLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LINK_REUSAR` | `linkReusarLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LINK_PRAZO` | `linkPrazoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_DET_ESPACO` | `detEspacoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_FOCO_ACENTO` | `focoAcentoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_NT_ORDEM` | `notaTituloOrdem` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_ARTE_TOM` | `artToneLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_PALCO_FUNDO` | `palcoFundoLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_URL_LISTA` | `-urlLista` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_URL_M3U_ADD` | `-urlM3uAdd` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_URL_XMLTV_ADD` | `-urlXmltvAdd` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_URL_REM` | `-urlRem` | (d) external store or action | See inventory below; action/status rows have no persistent value |
| `AJ_NT_EP` | `notaEpisodio` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |
| `AJ_LIVETV_SUGERIR` | `liveTvSugerirLocal` | (c) LOCAL, never backed up | ajustes.txt; excluded by ajustePortavel |
| `AJ_AUDIO_OFFSET` | `audioOffsetMsLocal` | (a) uploaded + restored | c_settings_v1.values; existing web fields also merged when supported |

## External stores

| Store | Baseline | Required recovery |
| --- | --- | --- |
| fonteregra.txt / profile copies | (a/d) addon order, regex, allowed addons/plugins in store0 | Shared |
| fileirasui-pN.txt | (a/d) rows/order/hidden/card/hero in store1; quota stripped | Shared rows plus exact same-TV rows/quota |
| fontefalha.txt / account-profile variant | (a/d) filters/fallback order/limits in store2 | Preserve provider scope and filename |
| player.txt | (a/d) primary style scalars covered; aspect/touched mask missing | Same-TV file recovery |
| player-externo.txt | (d) installed package choices, no backup | Same TV |
| notaTituloFontes, texturasEscolhaLocal, bibliotecaFonteEscolhidaLocal, idiomaAutoLocal, idiomaFonteLocal | (c) extra ajustes.txt records outside CHAVE | Explicit order and choice markers |
| selos-pN.txt, selos-pN-K.json | (d) imported badge rules/selection local; account rules pulled separately | Same TV |
| ltv-fontes-pN.txt, ltv-urls-pN.txt | (d) per-source player/groups and M3U/XMLTV URLs | Same TV |
| fanart.txt, seekr.txt, posteres.txt, p2p.txt, curadoria.txt | (d) keys/endpoints local | Same TV; account-private backup |
| stalker-pN.txt, xtream-pN.txt, jellyfin-pN.txt, emby-pN.txt, plex-pN.txt, discord-pN.txt | (d) service connections local | Same TV; restricted local permissions |
| debrid.txt / account-profile variant | (d) supported credentials use provider RPC; separate scope local | Same-TV scope/configuration |
| plugins-accountHash-pN.txt | (d) plugin repositories/selection local | Same TV |
| addons and account catalog order | (a/d) dedicated addon/catalog RPC and caches | Existing sync |
| TMDB/MDBList/debrid/Trakt credentials | (a/d) provider credential RPC | Existing sync |
| audiomemoria-pN.txt, legmemoria-pN.txt, fontepref-pN.txt, fontepref-auto-pN.txt | (d) remembered per-title audio/subtitle/source choices | Same-TV backup and reader invalidation |
| listas-pN.txt, salvos-org-pN.txt | (d) pinned lists and library presentation/categories | Same-TV backup and reload |
| enquete opt-out / searchable profile | (d) dedicated account service; flag mirrored locally | Scalar plus existing service |
| sessions, progress, watched, library, collection data, cache, logs, safe-mode/migration markers | Not configuration backup | Dedicated sync; never include session/cache/logs |

## Findings

- Login calls sync_reaplicar_ajustes: removes protection, clears outgoing pending state and blob, rearms apply/restore. sync_passo applies before lowering barrier. Logout resets this state. Failed pulls keep the barrier.
- Missing scalar keys use continue, preserving values. Partial blobs do not reset unrelated scalar settings.
- Existing sync_push_profile_settings_blob accepts arbitrary p_settings_json through the existing client contract. External RPC server source is not checked in. No new endpoint required; deployed-service roundtrip remains live validation.
- Excluded hardware keys never enter c_settings_v1. Reinstall deletes them. dados_cliente_id is an installation UUID in cliente.txt, so it cannot identify the TV after reinstall. Model alone collides across identical TVs.
- Replacing c_settings_v1 wholesale loses unknown extension fields. Several external editors never mark settings dirty.

## Implementation and evidence

The existing settings RPC now carries c_devices_v1, keyed by an opaque hardware identity. Android supplies ANDROID_ID (scoped to signing key and user); WGT supplies Samsung DUID with the productinfo privilege; native TV builds use a stable network hardware address. No model-only or installation UUID fallback is used. If identity is unavailable, shared preferences still sync and device preferences remain local. Hardware/network replacement or Android signing-key/user changes create a new device identity.

c_settings_v1 retains shared values and its existing three stores. Hardware blur/glass choices now use only the device section; legacy shared hardware entries are ignored. The device section contains every excluded persistent scalar, explicit provider scope, memory/library/language provenance, custom rating order, and 33 fixed file slots (including remembered audio/subtitle/source choices, pinned lists, and library categories). Sessions, logs, caches, safe-mode flags and arbitrary directory contents are excluded. Existing migration completion markers must also be stamped when a current cloud snapshot restores: a first run without ajustes.txt returns before those migrations, and the following launch otherwise resets resolution, appearance, sizing and language choices. Service credentials are account-private inside the device section (hex is encoding, not encryption); restored local files use the existing atomic 0600 writer. The allowlist, not remote property text, chooses every filename.

The merge overlays known JSON values and recursively preserves unknown shared fields, extension fields, device fields and other devices. Existing web value types and nesting remain intact. Partial blobs skip absent values/files. Explicit null file slots represent deletion; absent slots mean unchanged. Strict hex decoding rejects embedded NUL and malformed input. Cloud files are applied immediately to player, IPTV, server connections, source rules, badges, pinned lists and library presentation.

A main-thread snapshot comparison before each sync detects file-backed changes even when an editor has no sync hook, and marks durable pending protection. Closing Settings requests a sync. Login/profile/logout clears the comparison baseline and keeps the existing pull/apply barrier, so fresh defaults are not uploaded before restore. As before, a successful upload is required before uninstall or a network-disconnected logout; no client can recover data never delivered to the account. Concurrent whole-blob writes remain subject to the existing server last-writer behavior; there is no checked-in compare-and-swap contract. Older web/mobile clients that replace the entire blob can still remove extension fields; this TV preserves unknown fields when it writes.

FAIL proof: bash tests/ajustes_nuvem.sh returned 1 with `FAIL: external/device edits uploaded without dirty notification` before production changes. A stale HTTP double initially failed to link (missing nuvem_addon_alternar); the double was updated before obtaining the behavioral failure.

PASS proof: bash tests/ajustes_nuvem.sh returned 0. Separate processes/directories model uninstall; the test changes every persistent scalar, saves shared/device settings and external files, uploads without an explicit dirty notification, logs in as TV B, confirms shared values plus hardware/credential isolation, uploads TV B, then restores TV A twice from fresh data directories. It checks logout/login, player style/aspect, row quota, rating order, provider scope, explicit markers, partial blobs, legacy GPU isolation, unknown JSON retention, and 0600 credential permissions. HTTP alone is replaced; settings, disk, sessions and sync are production code.

Existing checks completed so far: ajustes_sync, addons_sync_ui, audsync, autosync, colfileiras_sync, dados-sync-rearm, legsync and syncprog returned 0; i18n returned 0. Existing failures: audsync_shot returned 134 (Android-only PCM expected on host); legsync_shot returned 134 (expected LEGSYNC_DESFEITA); autosync_corpus returned 1 (missing i18n); sync_aplicar_perf returned 1 (missing addon helper stubs); sync_prioridade returned 1 (duplicate/conflicting fixture definitions); sync_resumo_corrida, syncaddons, synccolecoes and syncordem returned 1 (stale fixtures omit nuvem_addon_alternar and/or traktauth_desconectado/addon helpers); tizen_fribb_asyncify returned 1 (expects literal stack size although build now uses ASYNCIFY_STACK). These failures occur outside the changed settings behavior. bash tools/build-local.sh returned 0 (existing ATOMIC_VAR_INIT warning); git diff --check returned 0. Extended next-launch regression first returned 1: restored resolution, profile background, appearance and scaling were reset by legacy migrations. The restore now stamps existing migration completion markers for current-schema snapshots, preserving restored choices on the following launch. Final bash tests/ajustes_nuvem.sh returned 0, including device-save, device-other, device-restore, device-reinstall and device-reopen. The final target recompiles production code after the migration fix; the full local build was run once before that final small fix, with a final syntax check also returning 0. Live server roundtrip and physical Android/WGT/TPK/webOS identity behavior are not exercised by host tests. No push, deploy or install performed.
