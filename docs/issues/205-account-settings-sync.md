# Account settings restoration (2.0.5)

Root causes:

- Existing merge updated only keys already present in the account blob. New accounts had no merge base; C-only preferences never uploaded.
- Logout purged profile copies and home rows locally; it never deleted the server blob. Missing cloud coverage made the next login lose those preferences.
- The worker pulled and pushed before the main thread applied settings. A local edit during initial restore could disable application and upload defaults. Persisted pending settings also were not re-marked for upload at startup.
- Several subtitle, source filter/order and home-row editors saved locally without marking account settings dirty.

The worker now requires a successful settings pull in the current cycle, no pending cloud application, and a completed login restore before uploading. Failed pulls retain the barrier. Confirmed empty results seed an empty base; failed or malformed replies do not. Login/profile reset clears outgoing pending state; profile-local unsent edits retain their existing explicit protection. Logout performs local cleanup only.

## Portable keys

Existing web keys retain their types and nesting. A separate root property, c_settings_v1, stores C preferences; values use c_ prefixes so recursive web readers cannot mistake them for web fields. This adds full support even when individual keys never existed in the account. Numeric values pass through existing option limits when restored.

210 persistent scalar keys on this build, plus the ten primary-subtitle style fields (c_-legenda1Tamanho, c_-legenda1Fonte, c_-legenda1Cor, c_-legenda1Opacidade, c_-legenda1Fundo, c_-legenda1Posicao, c_-legenda1Borda, c_-legenda1Atraso, c_-legenda1Negrito, c_-legenda1CorBorda):

```text
qualidade
legendaIdioma
audioIdioma
pauseOverlayEnabled
escolherFonteManual
fonteAutoLocal
fonteReporLocal
fonteTextoLocal
modernLandscapePostersEnabled
modernHeroFullScreenBackdropEnabled
heroFundoLocal
heroDifferentFromCard
trailerHero
collapseSidebar
modernSidebar
modernSidebarBlur
heroSectionEnabled
perfilFundoLocal
discoverLocation
posterLabelsEnabled
catalogAddonNameEnabled
catalogTypeSuffixEnabled
hideUnreleasedContent
homeImdbRatingsVisibility
classicFocusGradientEnabled
continueWatchingEnabled
cwOkLocal
cwFonteLocal
continueWatchingCardStyle
useEpisodeThumbnailsInCw
blurContinueWatchingNextUp
nextUpFromFurthestEpisode
showUnairedNextUp
continueWatchingSortMode
blurUnwatchedEpisodes
detailPageTrailerButtonEnabled
preferExternalMetaAddonDetail
showFullReleaseDate
detalheVeu
trailerAuto
trailerQualidade
trailerAspecto
trailerFonteLocal
focusedPosterBackdropExpandEnabled
focusedPosterBackdropExpandDelaySeconds
fastHorizontalNavigationEnabled
bordaFocoCartaz
cardDepthEnabled
cardDepthEdgeStrength
cardDepthSheenStrength
cardDepthEdgeCoverage
cardDepthPostersEnabled
cardDepthContinueWatchingEnabled
cardDepthEpisodeCardsEnabled
cardDepthCastEnabled
cardDepthTrailersEnabled
posterCardWidthDp
posterCardCornerRadiusDp
idioma
animacoes
selected_theme
corDaLogoLocal
epgPaisLocal
salvosDestino
tmdb_enabled
tmdb_language
tmdb_use_artwork
tmdb_use_basic_info
tmdb_use_details
tmdb_use_release_dates
tmdb_use_credits
tmdb_use_productions
tmdb_use_networks
tmdb_use_episodes
tmdb_use_trailers
tmdb_use_more_like_this
tmdb_use_collections
tmdb_enrich_continue_watching
mdblist_enabled
mdblist_show_trakt
mdblist_show_imdb
mdblist_show_tmdb
mdblist_show_letterboxd
mdblist_show_tomatoes
mdblist_show_audience
mdblist_show_metacritic
mdblist_show_mal
menuExplorarLocal
menuGuiaLocal
menuAgendaLocal
menuPerfilLocal
trailerCartazModo
fonteInterface
vidroLocal
homeLayoutLocal
soCinemetaLocal
notaTituloImdb
notaTituloTomates
notaTituloAudiencia
notaTituloMeta
notaTituloMetaUser
notaTituloTrakt
notaTituloTmdb
notaTituloLetter
notaTituloMal
notaTituloEbert
notaTituloScore
vidroContornoLocal
trailerDestaqueSomLocal
trailerDestaqueEsperaLocal
cwConcluidoLocal
addonsPrincipalLocal
heroTransicaoLocal
posterAddonLocal
fundoAddonLocal
logoAddonLocal
colArteContaLocal
selosColoridosLocal
trailerDetalheSomLocal
relogioTelaLocal
relogioPosLocal
seloVistoLocal
seekrLocal
seekrFitaLocal
seekrAjusteLocal
saidaPlayerLocal
reacaoCreditosLocal
fonteEsperaLocal
fundoLocal
vidroOpacLocal
vidroFoscoLocal
legendaSecundariaIdioma
avancadasLocal
legenda2PosLocal
legenda2EscalaLocal
legenda2CorLocal
legenda2FundoLocal
legenda2BordaLocal
fontePrioridadeLocal
fonteHdrLocal
enquetesLocal
buscaCinemetaLocal
descansoEstiloLocal
descansoFonteLocal
relogio12hLocal
tamanhoMaxLocal
tamanhoMinLocal
legendaForcadaLocal
classifPlayerLocal
streamAutoPlaySource
fonteOutrosLocal
fonteRegexLocal
espacoFileirasLocal
espacoTitulosLocal
cwRetidoTambemLocal
psContinuarLocal
histContaLocal
socialLocal
fonteOrdemUsoLocal
fonteTocarConferindoLocal
fonteAquecerLocal
fonteConferirVariasLocal
fontePrepararLocal
cwProximoLocal
buscaNuvioLocal
buscaOrigemLocal
ajustesLayoutLocal
fonteOrdemLocal
legendaSyncAutoLocal
legendaMostrarLocal
telaAberturaLocal
cwMinimoLocal
pularAberturaLocal
pularResumoLocal
proximoAutoLocal
pularCreditosLocal
proximoContagemLocal
menuTopoLocal
abasEstiloLocal
legendaContornoCorLocal
hideWatchedMoviesLocal
heroGeneroLocal
aberturaAddonLocal
heroSinopseLocal
heroNotasLocal
heroAnoLocal
heroDuracaoLocal
aberturaFundoLocal
heroSynopsisSizeLocal
epTamanhoLocal
fontePrecarregarLocal
muralTitulosLocal
friendsRowStyleLocal
fonteIdiomaAudioLocal
audioSecundarioIdioma
legenda2FonteLocal
legenda2OpacidadeLocal
legenda2PosicaoLocal
legenda2AtrasoLocal
legenda2NegritoLocal
legenda2CorBordaLocal
colecaoExibicaoPadraoLocal
trailerAudioLocal
aberturaEstiloLocal
menuCartazLocal
rowTitleStyleLocal
fundoDinamicaLocal
fundoDinamicaCorLocal
detEspacoLocal
focoAcentoLocal
pausaComentariosLocal
```

Three credential-free preference stores travel with the section, encoded as hexadecimal to preserve tabs/newlines exactly with the existing JSON reader:

- store0: fonteregra.txt: autoplay regex, allowed addon/plugin names, addon order.
- store1: fileirasui-pN.txt: home row order, hidden state, card style/size, collection item order, hero source. Row quota and account-owner marker stay local. Legacy row/hero identifiers containing URL delimiters are omitted because addon URLs can contain credentials.
- store2: active fontefalha preference file: provider/addon fallback order and limits, source filters. Addon instance IDs are hashes; no debrid keys or addon URLs.

## Device and secret exclusions

47 scalar keys remain local:

```text
dolbyVision
dolbyAtmos
qualidadeImagem
resolucaoUi
envioAuto
texturasMB
itensFileiraLocal
gpuEfeitosLocal
p2pLocal
posterProvLocal
liveTvResolucaoLocal
xtreamFormatoLocal
liveTvEsperaLocal
liveTvModoLocal
liveTvProxyLocal
medidorFormaLocal
tamanhoUiLocal
iconeAppLocal
tamanhoAjustesLocal
logoTrailerLocal
legendaSyncAudioLocal
cacheSeekLocal
trailerZoomTpkLocal
jellyfinLocal
logoAppLocal
aberturaAppLocal
esmaecerLocal
brilhoPlayerLocal
manterVideoLocal
proporcaoPadraoLocal
dtsSaidaLocal
dvMkvLocal
p2pLimiteLocal
p2pDestinoLocal
hdrUiLocal
bufferPlayerLocal
interfaceModoLocal
micBuscaLocal
tecladoAndroidLocal
retomadaMornaLocal
manterVideoPrazoLocal
estatTamanhoLocal
estatLadoLocal
playerExternoLocal
playerExternoAnimeLocal
vidroQualidadeLocal
escalaTextoLocal
```

Reasons: display resolution, HDR/OLED/aspect/zoom and decoder/audio compatibility belong to each TV; memory/image/row quotas, buffers/cache, blur quality and retention budgets depend on hardware; scale and TV/mobile/keyboard/microphone mode depend on screen/input; external players and launch icons require device-installed packages; personal-server, P2P, IPTV and poster-provider configuration depends on local endpoints/credentials; diagnostic overlays and log-upload consent belong to the device. Tizen uses dtsTpkLocal in place of dtsSaidaLocal.

All action/read-only rows are omitted. Credential stores remain unchanged: sessions, debrid/addon tokens, passwords, TMDB/MDBList/fanart/Seekr keys, IPTV and personal-server credentials, poster URLs/tokens. Imported badge-package URLs/recipes and package selection remain in their existing per-device stores: URLs may contain credentials; account-provided badge definitions still restore through the existing stream_badge_settings reader. No arbitrary data-directory backup.

## Evidence and limits

- bash tests/ajustes_sync.sh: real C sync/settings/session code with only HTTP replaced. Initial red failed at appearance/layout/language restoration; final passes roundtrip, defaults, empty, offline, and the existing account-blob compatibility cases. Roundtrip resets local settings and subtitle/autoplay/home-row/source-fallback stores after logout, then compares every portable scalar with its saved value and verifies cloud restore and no extra upload. Checks also cover unknown cloud features, display exclusion and credential exclusion.
- NUVIO_MAC_SO_COMPILAR=1 bash tools/mac.sh: rc 0. Existing incomplete server-config and ATOMIC_VAR_INIT warnings.
- python3 tools/varredura-i18n.py: rc 0, findings 0/0/0.
- git diff --check: rc 0.

Not exercised: live server persistence of the C extension, physical TVs, existing broad test suite. Older clients can ignore the extension; older clients that replace the entire blob may remove it. Settings edited while first restore is pending defer to the account snapshot. Pending edits must reach a successful push before logout to be available on another TV.
