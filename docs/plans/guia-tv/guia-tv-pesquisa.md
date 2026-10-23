# Guia de TV / Live TV — pesquisa (links)

Pesquisa feita em 06/10/2026 para o plano `guia-tv-plano.md`. Duas partes:

1. **Apps de referência e UX** (TiviMate, IPTV Smarters, Sparkle, OTT Navigator, Kodi IPTV Simple, Channels DVR, Plex, Jellyfin, Pluto/Samsung TV Plus/LG Channels, Apple TV, YouTube TV, Sling, Google TV) — o que fazem, matriz de recursos, padrões consagrados e reclamações.
2. **Técnica** (XMLTV, M3U estendido, Xtream Codes API, casamento de EPG, Stremio/Nuvio Live TV, reprodução por plataforma, fuso horário) — fatos com fonte e recomendações numéricas.

Avisos de confiabilidade das fontes:
- Reddit e páginas oficiais de TiviMate/Plex/YouTube TV/Sling não abriram (403/paywall); várias fontes de IPTV são blogs de revendedor (marcadas "SEO", confiança baixa). Células "n.v." = não verificado.
- Itens marcados **[não verificado]** não tiveram fonte direta. Números das recomendações são decisão de projeto amarrada às fontes, não valores prescritos por elas.

Fontes-chave (atalho):
- Kodi pvr.iptvsimple (match em 3 passes, tvg-shift, instâncias por fonte): https://github.com/kodi-pvr/pvr.iptvsimple/blob/Omega/README.md
- Channels DVR (On Now, Collections, Jump to Last Channel, Surfing): https://getchannels.com/docs/apps/usage/browsing-whats-on/ · https://getchannels.com/docs/channels-dvr-server/how-to/channel-collections/
- Stremio EPG em add-ons (`behaviorHints.epgProvider`, `meta.videos` com startTime/endTime): https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/epg.md
- NuvioTV oficial: pedido de aba Live TV em aberto: https://github.com/NuvioMedia/NuvioTV/issues/2619 · forks com IPTV: https://github.com/killamfkr/NuvioTV-live
- Xtream API: https://github.com/worldofiptvcom/xtream-codes-api-documentation · catch-up/timeshift: https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API
- XMLTV/M3U specs práticas: https://uniplayer.net/docs/specs/xml/ · https://uniplayer.net/docs/specs/m3u/
- Expat (parse em stream): https://libexpat.github.io/doc/api/latest/
- webOS streaming: https://webostv.developer.lge.com/develop/specifications/streaming-protocol-drm · Tizen AVPlay: https://developer.samsung.com/smarttv/develop/api-references/samsung-product-api-references/avplay-api.html · Media3 live: https://developer.android.com/media/media3/exoplayer/live-streaming

---

# Parte 1 — Apps e UX

Data: 2026-10-06. Objetivo: embasar um Guia de TV ao vivo "profissional, confiável, organizado" no app nativo (LG webOS, Samsung Tizen, Android TV; D-pad).

## Nota de confiabilidade das fontes (ler primeiro)

- Fontes **primárias** (docs oficiais, suporte do produto): Channels DVR (getchannels.com), Samsung Support, Kodi GitHub/issue tracker, Jellyfin docs, Xtream wiki. Confiança alta.
- Fontes **secundárias boas**: 9to5google, Droid Life, Android Police, Pocket-lint, TechHive, TROYPOINT forum.
- Fontes **fracas** (blogs SEO de revendedores de IPTV, "guias 2026"): usadas só para fatos de configuração que se repetem em várias fontes (ex.: update interval, "past days to keep EPG"). Marcadas com (SEO).
- **Não consegui ler Reddit** (r/TiviMate, r/IPTV) nem as páginas oficiais do TiviMate/Plex/YouTube TV (403/vazio/paywall `tollbit`). As reclamações abaixo vêm de fóruns (TROYPOINT, Kodi tracker), docs e guias de troubleshooting — não de threads do Reddit. Onde a matriz diz "não verificado", não achei fonte.
- Apple TV: **não existe guia de canais ao vivo nativo** no app Apple TV (só aba Sports/Live de eventos). Isso é conclusão a partir das fontes; não achei nada de "live guide tvOS 18" da Apple.

---

## 1. TiviMate (referência)

Só Android TV / Google TV / Fire TV; não existe para iOS/Apple TV/Roku/Web. https://iptvrepair.com/tivimate/

- Layout: grade EPG estilo cabo (time grid) + lista de canais; XMLTV e EPG do provedor. https://iptvrepair.com/tivimate/ , https://tivimate.co.com/stream/features/
- Free vs Premium: free = 1 playlist, EPG, grupos básicos; Premium = múltiplas playlists, catch-up, gravação, multiview, favoritos/grupos customizados completos. Lifetime ~US$34 (5 dispositivos) ou ~US$10-12/ano (SEO). https://iptvrepair.com/tivimate/ ; outra fonte diz US$4,99/ano https://iptv-service-subscription.us/tivimate/tivimate-multiple-accounts/ (preços divergem entre fontes).
- Grupos: esconder, renomear e reordenar grupos; criar grupos customizados e favoritos. https://tivimate.co.com/stream/features/
- Multi-playlist: Premium; "até 10 contas" (SEO); canais são mesclados na interface principal, mas é possível separar por provedor; troca rápida entre playlists. https://tivimateiptvplayer.net/manage-multiple-playlists-in-tivimate/ , https://tivimate.co.com/stream/features/
- Busca embutida: canais, filmes, séries, VOD. https://tivimate.co.com/stream/features/
- EPG: fontes múltiplas; **offset de tempo por fonte** (Settings > EPG > EPG sources > Time offset) e **offset por canal** (long-press OK > opções do canal). https://tivimateguide.com (resultado de busca citando) — texto: "TiviMate Player: Adjusting EPG Time Offset" https://tivimateguide.com/knowledgebase/tivimate-player-adjusting-epg-time-offset/ (página retornou 404 ao fetch; fato vem do snippet de busca).
- EPG refresh: intervalo configurável; recomendação comum 12-24 h (SEO), "update on app start" e "past days to keep EPG" (ex.: 7 dias; 1-3 em aparelho fraco). https://troypointinsider.com/t/tivimate-epg-only-populates-24-hrs-in-advance/79283 , https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/
- Match EPG: 3 níveis — `tvg-id` (case-sensitive), `tvg-name`, display-name; usuário pode editar o "EPG ID" do canal manualmente (Edit channel). https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/ , https://m3u.codes/guides/epg-not-showing/
- Catch-up (Premium, se o provedor suportar), gravação local/rede, timeshift (pausar/rebobinar), multiview. https://iptvrepair.com/tivimate/
- Parental: PIN em canais e categorias. https://tivimate.co.com/stream/features/
- Player externo (MX, VLC); logos/layout EPG/temas personalizáveis. https://tivimate.co.com/stream/features/
- Buffer: 8-15 s recomendado; acima de 15-20 s piora em aparelho de pouca RAM. https://tivimateplayer.co.uk/tivimate-buffering-fix/ (SEO)
- Limites de performance: playlists grandes + intervalo de EPG agressivo (a cada 2 h) causam "database locked", lag e crash; XMLTV descomprimido pode estourar RAM em aparelhos de 1-1,5 GB. https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/ (SEO)
- Reclamações: buffering após update 5.2; playlists enormes demoram a parsear. (Trustpilot via busca, sem fetch) https://uk.trustpilot.com/review/tivimate.org

## 2. IPTV Smarters Pro

- Formatos: M3U, Xtream Codes, Stalker; Android, iOS, Fire TV, Windows, "alguns Samsung via Smart Hub"; nenhuma menção a LG na fonte lida. https://hiiptv.com/iptv-player-apps/iptv-smarters-pro
- EPG com agora/próximo por canal; multi-screen até 4 canais; catch-up 24-72 h dependendo do provedor; PIN parental por canal/categoria; gravação em armazenamento externo em aparelhos compatíveis. https://hiiptv.com/iptv-player-apps/iptv-smarters-pro
- Falhas de parse de XMLTV grande são citadas como problema recorrente neste app. https://ott-tv.org/blog/xmltv-epg-explained/
- Detalhes de layout (grade vs lista) e separação multi-playlist: **não verificado**.

## 3. Sparkle TV (Hede Konsult)

- Suporta m3u, Xtream, XMLTV; guia + guia de canais com logos e imagens de programa. https://www.appbrain.com/app/sparkle-tv-iptv-player/se.hedekonsult.sparkle (resumo de busca)
- Favoritos, ordenar canais e categorias, mostrar/esconder canais e categorias; integração com tela inicial do Android TV. (idem)
- Busca por canais/programas/filmes/séries; DVR, timeshift, multiview, VOD com catch-up no plano Plus. (idem)
- Fonte limitada (resumo de busca de lojas de apps); nada sobre limites de EPG.

## 4. OTT Navigator

- Playlists M3U, Xtream, Stalker/MAC; roda em Android TV/Fire TV (sideload no Fire). https://ottnavigator.help/ , https://iptvsenpai.com/players/ott-navigator/
- Diferencial: **trata várias playlists como um serviço** — mescla canais e EPG em uma lista; free limita 1-2 provedores, Premium ilimitado (SEO). https://bestiptvservices.net/ott-navigator (resultado de busca)
- Visualizações de EPG: grade, lista e tile; muita personalização visual (cor, fundo, formato de hora). https://iptvsenpai.com/players/ott-navigator/
- Lista: categorizar, esconder adulto, opções de ordenação. https://iptvsenpai.com/players/ott-navigator/
- Ausências citadas na review: sem multi-screen, sem DVR. (idem)
- Catch-up: suportado segundo outras fontes (resultado de busca), review lida não menciona.

## 5. Kodi — PVR IPTV Simple Client + janela de guia

- Kodi 20+: **múltiplas instâncias do addon**, cada uma com seu par M3U + XMLTV (ex.: grátis + pago, ou por idioma, todos no mesmo guia). https://github.com/kodi-pvr/pvr.iptvsimple , https://shop.plume.com/blogs/news/iptv-on-kodi
- Atributos M3U: `tvg-id`, `tvg-chno` (número do canal), `group-title` (vários grupos separados por `;`), `catchup` (default/append/shift/vod), `tvg-shift` (horas, por arquivo ou por canal), `tvg-logo`, `radio`. https://github.com/kodi-pvr/pvr.iptvsimple
- Match EPG em **3 passes**: `id` do XMLTV = `tvg-id`; senão `display-name` = `tvg-name`; senão nome do canal. https://github.com/kodi-pvr/pvr.iptvsimple
- XMLTV pode vir em gzip/XZ; tem timeshift e catchup com dias configuráveis. https://github.com/kodi-pvr/pvr.iptvsimple
- EPG time shift global de -12 h a +14 h; opção de aplicar a todos os canais. https://shop.plume.com/blogs/news/iptv-on-kodi (SEO) , https://forum.kodi.tv/showthread.php?tid=219835
- Guia: grade rolável (TV > Guide) no skin Estuary; grupos de canais, gravação via EPG com addon extra. https://koditips.com/set-up-an-epg-on-kodi/ , https://forum.kodi.tv/showthread.php?tid=329991
- Problema real: reimporta o EPG a cada reinício, ~5 min de "Importing Guide" com ~100 MB de XML em Raspberry Pi 3, mesmo com intervalo de 600 h; issue fechada "not planned". https://github.com/xbmc/xbmc/issues/19107
- Desenvolvedor de skin relata dificuldade em expor grupos de canais selecionáveis no guia. https://forum.kodi.tv/showthread.php?tid=334837 (do snippet; fetch 403)

## 6. Channels DVR (Channels app)

- Telas: **On Now** (parede de arte do que passa agora) + **Guide** (grade tradicional). https://getchannels.com/docs/apps/usage/browsing-whats-on/
- **Filtros persistentes** ("sticky"): Favoritos, HD/SD, por conteúdo atual (Filmes, Esportes, Notícias, Drama, Kids). https://getchannels.com/docs/apps/usage/browsing-whats-on/
- **Channel Collections**: grupos de canais criados no servidor, aplicados a todos os clientes ou por cliente; ordem das coleções = ordem de inclusão; **regras automáticas** (gênero do que passa agora, fontes excluídas, tipo de conteúdo, tags Live/New/HD/CC, palavra-chave na descrição). https://getchannels.com/docs/channels-dvr-server/how-to/channel-collections/
- Favoritos reordenáveis (segurar Select > "Rearrange Channels"). https://getchannels.com/docs/apps/usage/browsing-whats-on/
- Configurações: "Lineup Order", "Stack Duplicates" (por número de canal), "Channel Surfing" (quais canais entram no zapping), **Jump to Last Channel (duplo-play)**, Up/Down para zapping no Siri Remote 2, botões coloridos configuráveis (Guide = verde), **Buffer Protection** (avisa antes de sair se não estiver no ao vivo). https://getchannels.com/docs/apps/usage/settings/
- Mini-guide em overlay durante o player, filtrável por gênero e favoritos. https://getchannels.com/live-tv/ (resultado de busca)
- 21 dias de guia, atualizado diariamente; busca; gravação a partir do guia; Series Pass, padding, auto-delete. https://getchannels.com/live-tv/ (resultado de busca)
- Ocultar canais por fonte (Manage lineup: coração/remover). https://www.techhive.com/article/1069736/advanced-cord-cutting-tricks-for-channels-dvr.html (resultado de busca)
- Canal sem dados de guia não grava (reclamação comum). https://community.getchannels.com/t/channels-without-guide-data-cannot-be-recorded/32864

## 7. Plex Live TV & DVR

- Guia em grade, filtros por gênero; múltiplas fontes de TV ao vivo (DVR + Plex free) podem ser ligadas/desligadas no guia. https://support.plex.tv/articles/225877387-program-guide/ (resultado de busca; página 403 no fetch)
- Favoritos de canais de qualquer fonte, com ordem customizável. (idem)
- Manage Channels: esconder (ícone de olho) e reordenar; **comentários 2022-2025 dizem que o procedimento deixou de funcionar** — regressão de UX. https://www.aftvnews.com/how-to-hide-and-or-rearrange-live-tv-channels-in-plexs-program-guide/
- "Add channels from another lineup" (CEP vizinho) para antena. https://support.plex.tv/articles/225877387-program-guide/
- Suporte a IPTV é indireto (via Telly/Threadfin etc.). https://github.com/tellytv/telly/wiki/Adding-Telly-to-Plex

## 8. Jellyfin Live TV

- Tuners: HDHomeRun e M3U; guia: Schedules Direct **ou** XMLTV (não os dois ao mesmo tempo). https://jellyfin.org/docs/general/server/live-tv/setup-guide/
- **Mapeamento manual** de canal físico para canal do guia (ícone de lápis) — fricção alta. (idem)
- Limite de streams simultâneos configurável; header/User-Agent customizável; auto-loop. (idem)
- Tuner M3U embutido: **uma única playlist**, sem buffer, sem gestão avançada de canais, sem monitor de saúde do stream; recomenda-se Threadfin (proxy) para multi-playlist. https://jellywatch.app/blog/jellyfin-m3u-tuner-live-tv-without-hardware-threadfin-dispatcharr-2026 (resultado de busca, SEO)
- Doc oficial não especifica dias de guia nem intervalo de refresh. https://jellyfin.org/docs/general/server/live-tv/

## 9. FAST: Pluto TV / Samsung TV Plus / LG Channels

**Pluto TV**
- Guia com **categorias** à esquerda/topo: Last Watched, Favorites, Featured, Movies, Comedy, News, Sports, Kids, etc. (25+). https://www.pocket-lint.com/pluto-tv-tips-and-tricks/ , https://www.pocket-lint.com/pluto-tv-tips-and-tricks/
- Favoritos via botão ao abrir o canal; aba Favorites no topo do guia; "Last Watched" exige conta. (idem) , https://support.pluto.tv/hc/en-us/articles/360007070372-Add-Channels-to-a-Favorites-List
- **Sem números de canal** no guia (decisão consciente). (idem)
- Busca distingue VOD ("watch now") de ao vivo ("Live"), com restart em alguns. (idem)

**Samsung TV Plus**
- Lista de canais: segurar botão CH 1-2 s; modelos 2025 abrem guia com Up/Down. https://www.samsung.com/au/support/tv-audio-video/add-channels-to-your-favourites-on-your-samsung-tv
- Favoritos: 2022-2024 via Edit Channels > Favourites > Add Channels; 2025 seleciona direto no guia ("Added to Favourites"); dá para **remover, reordenar e renomear** a lista; 2025 filtra guia só por favoritos (coluna da esquerda). (idem)
- Categorias: news, movies, sports, kids, lifestyle etc. https://samsungtechwin.com/samsung-tv-plus-channel-list/ (resultado de busca, SEO)

**LG Channels**
- Botão Guide abre grade (tempo horizontal, canais vertical); filtros Favorites/HD/categorias; favoritos por estrela/coração. https://www.lg.com/uk/lg-experience/helpful-hints/lg-smart-tv-channels/ (resultado de busca) , https://www.lg.com/us/support/help-library/lg-tv-how-to-save-favorite-channels--20153194206455
- Disponível em webOS 4.5+; 400-700+ canais conforme região. (resultados de busca)

## 10. Apple TV app (tvOS 18+)

- Não há grade de canais lineares no app Apple TV; existe aba **Sports** com jogos ao vivo/agendados (EUA/Canadá), "Up Next" e notificações de jogos. https://support.apple.com/guide/tv/play-live-content-atvb52f75784/17.0/tvos/17.0 , https://www.imore.com/how-watch-live-sports-apple-tv
- Guia de canais no tvOS vem de apps de terceiros (Channels, Tablo). https://getchannels.com/apple-tv/ , https://www.tablotv.com/for-appletv/
- tvOS 18 trouxe InSight, não guia ao vivo. https://tomsguide.com/tvs/apple-tvsos-18-is-live-with-new-insight-5-features-to-try-now
- Conclusão: Apple não é referência de guia; referência no tvOS é o Channels.

## 11. YouTube TV

- Guia customizável: **esconder, favoritar (sobe) e reordenar por drag** — feito no web em Settings > Live guide; persiste em todos os dispositivos; no mobile usa checkmarks; em TV há um ícone de 3 linhas na tela Live com opção "Custom". https://9to5google.com/2022/12/15/customize-channels-you-see-in-youtube-tv/ , https://support.google.com/youtubetv/community-guide/373293152/how-to-customize-your-guide-with-your-favorite-channels?hl=en (resumo de busca)
- Esconder canal **não bloqueia** assistir nem recomendações. (idem)
- Filtros do guia: Recommended, Sports, News, TV shows, Movies. https://www.androidpolice.com/youtube-multiview-tutorial/ (resultado de busca)
- **Multiview** (2-4 streams): botão "para baixo" no player abre o botão de multiview; D-pad troca o áudio; Select = tela cheia; Back = volta ao multiview; no início só combinações curadas, desde 2024 dá para montar customizado. https://www.androidpolice.com/youtube-multiview-tutorial/ , https://www.androidcentral.com/streaming-tv/youtube-tv/youtube-tv-fully-customizable-multiview-rolling-out
- Observação de design: a customização principal fica no web, não na TV (fricção para D-pad).

## 12. Sling TV

- **My Channels ribbon** no topo do guia; adicionar via botão "+" no fim da faixa; coração por canal (ou `*` no Roku). https://thestreamable.com/how-to-customize-sling-tv-guide (resultado de busca)
- Filtros do guia: Sports, Kids, Lifestyle, Premium etc.; esconder duplicados (só versão Blue) e "Display Unsubscribed Content" off. (idem)
- **Mini guide**: pressionar "para baixo" duas vezes durante a reprodução; padrão mostra canais recentes, com abas para Sports e lista completa. (idem)
- Reclamação de update recente que piorou o guia (artigo de opinião, não consegui ler): https://www.tomsguide.com/uk/opinion/the-new-sling-tv-update-is-so-bad-i-want-to-cancel-my-subscription

## 13. Google TV — aba "Live"

- Grade unificada de canais de FAST (Pluto, Tubi) e pay TV (YouTube TV, Sling). https://www.techhive.com/article/1924120/make-the-most-of-google-tvs-live-tab-with-these-tips.html (resultado de busca)
- Favoritos por estrela ao selecionar o nome do canal; abas "All channels" e "Favorite channels" (maio/2022); a reportagem notou a UI "sluggish". https://www.droid-life.com/2022/05/04/google-tvs-live-guide-gets-a-favorite-channels-tab/
- Filtros por gênero na coluna esquerda (News & Opinion, Movies, Classic TV). https://www.androidpolice.com/how-i-arranged-google-tv-streamer-free-channels-into-favorite-shows/ (resultado de busca)

---

## Fatos técnicos transversais (EPG / Xtream)

- Xtream: `get_short_epg&stream_id=..&limit=N`; canal tem `tv_archive` (0/1) e `tv_archive_duration` (dias). https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API
- URL de timeshift: `/timeshift/USER/PASS/DURAÇÃO_MIN/YYYY-MM-DD:HH-MM/STREAM_ID.ext`, horário de início em UTC (segundo a fonte); alternativa `streaming/timeshift.php?...`. https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API , https://forum.kodi.tv/showthread.php?tid=351431&page=33
- XMLTV: 50-200 MB descomprimido para milhares de canais em multi-dias; gzip ~8-20 MB; atualização diária basta; arquivo cobre 7-14 dias. https://ott-tv.org/blog/xmltv-epg-explained/ (SEO)
- Timestamp XMLTV `20260501100000 +0000`; se o offset faltar, o player "chuta" e dá erro de 5-8 h. https://ott-tv.org/blog/xmltv-epg-explained/
- tvheadend adicionou modo de EPG reduzido para TVs com pouca memória (sem descrições longas). https://github.com/tvheadend/tvheadend/pull/1275

---

## Matriz de funcionalidades

Legenda: sim / não / parcial / n.v. (não verificado nas fontes lidas). Células de apps de IPTV baseadas em fontes SEO têm confiança menor.

| Funcionalidade | TiviMate | IPTV Smarters | Sparkle | OTT Navigator | Kodi IPTV Simple | Channels DVR | Plex | Jellyfin | Pluto/FAST | Apple TV app | YouTube TV | Sling | Google TV Live |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Grade de tempo | sim | sim | sim | sim (grade/lista/tile) | sim | sim | sim | sim | sim | não | sim | sim | sim |
| Agora/próximo | sim | sim | sim | sim | sim | sim | sim | sim | sim | parcial (esportes) | sim | sim | sim |
| Mini-guide/overlay no player | n.v. | n.v. | n.v. | n.v. | n.v. | sim | n.v. | n.v. | n.v. | não | sim | sim (2x baixo) | n.v. |
| Grupos/categorias | sim | sim | sim | sim | sim (group-title) | sim (collections) | parcial (gênero) | parcial | sim | não | parcial (filtros) | sim (filtros) | sim (gênero) |
| Separação por playlist/fonte | parcial (mescla, dá p/ separar) | n.v. | n.v. | não (mescla por padrão) | sim (instâncias separadas) | parcial (excluir fontes, regras) | parcial (liga/desliga fonte) | não (1 M3U) | n/a | n/a | n/a | n/a | parcial (por serviço) |
| Favoritos | sim | sim | sim | sim | sim | sim | sim | parcial | sim | n/a | sim | sim | sim |
| Listas customizadas/reordenar | sim | n.v. | sim | sim | parcial | sim (rules + ordem) | parcial (regrediu) | não | não | n/a | sim (web) | parcial | não |
| Numeração de canais | sim | sim | n.v. | n.v. | sim (tvg-chno) | sim | sim | sim | não (decisão) | n/a | não | não | não |
| Busca canais+programas | sim | parcial | sim | n.v. | parcial | sim | sim | sim | sim | parcial | sim | sim | sim |
| Catch-up (tv_archive) | sim (Premium) | sim (24-72h) | sim (Plus) | sim | sim | não | não | não | parcial (restart) | não | sim (restart em alguns) | parcial | n.v. |
| Lembretes | n.v. | n.v. | n.v. | n.v. | parcial | n.v. | n.v. | n.v. | n.v. | sim (notif. esportes) | sim | n.v. | n.v. |
| Gravação | sim (Premium) | parcial | sim (Plus) | não | parcial (addon) | sim | sim | sim | não | não | sim (cloud) | sim | n.v. |
| Multi-EPG / tvg-id | sim (match 3 níveis, edita EPG ID) | n.v. | n.v. | sim | sim (3 passes) | n/a | n/a | parcial (mapeamento manual) | n/a | n/a | n/a | n/a | n/a |
| Refresh configurável | sim | n.v. | n.v. | n.v. | sim (reimporta no start) | diário (servidor) | servidor | automático (sem intervalo doc.) | n/a | n/a | n/a | n/a | n/a |
| Offset de fuso EPG | sim (fonte+canal) | n.v. | n.v. | n.v. | sim (-12/+14 h, por canal) | n/a | n/a | n.v. | n/a | n/a | n/a | n/a | n/a |
| Logos | sim | sim | sim | sim | sim | sim | sim | sim | sim | n/a | sim | sim | sim |
| Preview/PiP do canal focado | n.v. | n.v. | n.v. | n.v. | parcial (skin) | parcial (On Now em arte) | n.v. | n.v. | n.v. | n/a | parcial (multiview) | n.v. | n.v. |
| Último canal (recall) | n.v. | n.v. | n.v. | n.v. | n.v. | sim (duplo-play) | n.v. | n.v. | n.v. | n/a | n.v. | n.v. | n.v. |
| Entrada numérica no controle | n.v. | n.v. | n.v. | n.v. | sim (Kodi) | n.v. | n.v. | n.v. | não | n/a | n.v. | n.v. | n.v. |
| Zapping Up/Down no player | sim | sim | sim | sim | sim | sim (configurável) | sim | sim | sim | n/a | sim | sim | sim |
| Tratamento de stream morto | n.v. | n.v. | n.v. | n.v. | n.v. | parcial (buffer protection) | n.v. | não (sem monitor de saúde) | n.v. | n/a | n.v. | n.v. | n.v. |
| Controle parental | sim (PIN) | sim (PIN) | n.v. | sim (esconde adulto) | n.v. | n.v. | sim | sim | n.v. | n/a | sim | sim | n.v. |
| Multiview | sim (Premium) | sim (4) | sim (Plus) | não | não | não | não | não | não | n/a | sim (2-4) | n.v. | n.v. |
| Config. de dias de EPG | sim (past days) | n.v. | n.v. | n.v. | sim | 21 dias (servidor) | n.v. | n.v. | n/a | n/a | n/a | n/a | n/a |

Observação: "sim" em "Zapping Up/Down", "Logos", "Busca" para apps de IPTV é inferência de convenção do segmento mais do que citação direta; tratar como baixa confiança.

---

## Padrões de UX consagrados

1. **Dois modos complementares**: grade completa (planejar) + mini-guide em overlay (zapping). Channels e Sling fazem os dois; Sling abre com "baixo, baixo". Channels: https://getchannels.com/live-tv/ ; Sling: https://thestreamable.com/how-to-customize-sling-tv-guide
2. **"On Now" visual além da grade** (parede de arte) para descobrir sem decifrar tabela. https://getchannels.com/docs/apps/usage/browsing-whats-on/
3. **Favoritos como primeira aba/ribbon** do guia (Pluto, Google TV, Sling, Samsung 2025). https://www.droid-life.com/2022/05/04/google-tvs-live-guide-gets-a-favorite-channels-tab/
4. **Filtros persistentes ("sticky")** lembrados entre sessões. https://getchannels.com/docs/apps/usage/browsing-whats-on/
5. **Coleções/grupos definidos pelo usuário com ordem própria**, idealmente com regras automáticas (gênero, HD, fonte). https://getchannels.com/docs/channels-dvr-server/how-to/channel-collections/
6. **Esconder canal sem apagar** (hide != block) e reordenar; YouTube TV, Plex, Sparkle, TiviMate. https://9to5google.com/2022/12/15/customize-channels-you-see-in-youtube-tv/
7. **Separação por fonte/playlist como instância de primeira classe** (Kodi) ou como mescla com opção de separar (TiviMate/OTT). Regra prática: cada playlist com seu EPG e sua configuração de offset. https://github.com/kodi-pvr/pvr.iptvsimple , https://tivimateiptvplayer.net/manage-multiple-playlists-in-tivimate/
8. **Match de EPG em cascata** (tvg-id > tvg-name > nome) + override manual do EPG ID no canal. https://github.com/kodi-pvr/pvr.iptvsimple , https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/
9. **Offset de fuso por fonte E por canal** (TiviMate, `tvg-shift` do Kodi). https://tivimateguide.com/knowledgebase/tivimate-player-adjusting-epg-time-offset/ , https://github.com/kodi-pvr/pvr.iptvsimple
10. **"Último canal" com um gesto** (duplo-play no Channels) e Up/Down de zapping configurável sobre um conjunto escolhido ("Channel Surfing"). https://getchannels.com/docs/apps/usage/settings/
11. **Botões coloridos como atalhos** (guia = verde no Channels). https://getchannels.com/docs/apps/usage/settings/
12. **Retenção de EPG configurável** ("past days to keep") + atualização na abertura + intervalo diário; guardar 1-3 dias em aparelho fraco. https://troypointinsider.com/t/tivimate-epg-only-populates-24-hrs-in-advance/79283 , https://ott-tv.org/blog/xmltv-epg-explained/ (SEO)
13. **Catch-up dirigido por flag do canal** (`tv_archive` + `tv_archive_duration`), mostrando o ícone de arquivo só nos programas dentro da janela. https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API
14. **Multiview com D-pad simples**: baixo abre, D-pad troca áudio, Select tela cheia, Back volta. https://www.androidpolice.com/youtube-multiview-tutorial/
15. **Proteção ao sair do ao vivo** (Buffer Protection) e consistência entre dispositivos (YouTube TV sincroniza customização). https://getchannels.com/docs/apps/usage/settings/ , https://9to5google.com/2022/12/15/customize-channels-you-see-in-youtube-tv/

---

## Reclamações mais comuns

1. **EPG "No Information" por `tvg-id` divergente** (case-sensitive) entre M3U e XMLTV. https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/ , https://m3u.codes/guides/epg-not-showing/
2. **Horários errados** por fuso/offset ausente no XMLTV (erros de 5-8 h). https://ott-tv.org/blog/xmltv-epg-explained/
3. **EPG curto (8-24 h)** — depende do que o provedor entrega; usuários sugerem `&next_days=7`. https://troypointinsider.com/t/tivimate-epg-only-populates-24-hrs-in-advance/79283
4. **Reimportação lenta do EPG a cada abertura** (Kodi: ~5 min, ~100 MB, travando em Pi 3). https://github.com/xbmc/xbmc/issues/19107
5. **Crash/lag com playlist grande e refresh agressivo** ("database locked", RAM 1-1,5 GB estoura ao descomprimir). https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/ (SEO)
6. **Buffering/regressão após updates** (TiviMate 5.2) e cache que incha com o tempo. https://uk.trustpilot.com/review/tivimate.org , https://tivimateplayer.co.uk/tivimate-buffering-fix/
7. **Mapeamento manual de canal para guia** (Jellyfin) e tuner M3U de uma playlist só, sem saúde de stream. https://jellyfin.org/docs/general/server/live-tv/setup-guide/ , https://jellywatch.app/blog/jellyfin-m3u-tuner-live-tv-without-hardware-threadfin-dispatcharr-2026
8. **Customização que some ou quebra** (Plex Manage Channels; comentários dizem que deixou de funcionar). https://www.aftvnews.com/how-to-hide-and-or-rearrange-live-tv-channels-in-plexs-program-guide/
9. **UI lenta** na aba Live do Google TV ("sluggish"). https://www.droid-life.com/2022/05/04/google-tvs-live-guide-gets-a-favorite-channels-tab/
10. **Customização longe do controle remoto** (YouTube TV: principal no web) e atualizações de guia que pioram a navegação (Sling). https://9to5google.com/2022/12/15/customize-channels-you-see-in-youtube-tv/ , https://www.tomsguide.com/uk/opinion/the-new-sling-tv-update-is-so-bad-i-want-to-cancel-my-subscription
11. (extra) **Canal sem dado de guia não pode ser gravado/agendado** (Channels). https://community.getchannels.com/t/channels-without-guide-data-cannot-be-recorded/32864
12. (extra) **Free limita a 1 playlist; multi-playlist/catch-up/gravação só no pago** (TiviMate, OTT Navigator). https://iptvrepair.com/tivimate/ , https://bestiptvservices.net/ott-navigator

---

## Lacunas desta pesquisa

- Threads do Reddit (r/TiviMate, r/IPTV) e docs oficiais do TiviMate/Plex/YouTube TV/Sling não foram lidos; recomendado checar com acesso direto antes de decisões finais de prioridade.
- Faltam fatos sobre: tratamento de stream morto (retry/skip), número exato de linhas visíveis/tempo de rolagem em hardware fraco, lembretes em apps de IPTV, e PiP/preview do canal focado em apps de IPTV.

---

# Parte 2 — Técnica

Pesquisa de 06/10/2026 para um app nativo em C (SDL2 + GLES2) em LG webOS, Samsung Tizen e Android TV.
Cada fato traz a URL onde foi encontrado. Itens marcados **[não verificado]** não tiveram fonte direta. Números sem fonte são decisão de projeto e aparecem só em "Recomendações".

## 1. XMLTV

- Formato aberto, regido por DTD. O arquivo tem `<channel>` e depois `<programme>`. Cada programme leva `start`, `stop` e `channel` como atributos. — https://uniplayer.net/docs/specs/xml/
- Hora no formato `YYYYMMDDHHMMSS ±HHMM`, por exemplo `20260501100000 +0000`. Sem offset, o consumidor assume UTC. — https://uniplayer.net/docs/specs/xml/ ; https://ott-tv.org/blog/xmltv-epg-explained/
- `<channel>` exige `display-name` e aceita `icon` com a URL do logo. — https://uniplayer.net/docs/specs/xml/
- O problema de horário mais comum é o guia deslocado em 5 a 8 horas, ou em exatamente 1 hora. A causa é offset ausente, offset errado, ou arquivo em hora local rotulado como UTC. — https://ott-tv.org/blog/xmltv-epg-explained/ ; https://virginmediauk.com/step-by-step-epg-xmltv-sync-guide-keep-your-iptv-showtimes-correct/
- Outros defeitos que derrubam canais: ids de canal duplicados, programme que referencia canal inexistente, `stop` antes de `start`, buracos na grade e pouca profundidade futura. — https://www.ottengine.com/tools/xmltv-epg-validator ; https://ott-tv.org/blog/xmltv-epg-explained/
- Tamanho. Descompactado vai de 50 a 200 MB, e em .xml.gz de 8 a 20 MB. — https://ott-tv.org/blog/xmltv-epg-explained/
  - Exemplos reais do epgshare01: US2 tem 6,4 MB em gzip e 76,7 MB em XML, com 766 canais. US_LOCALS1 tem 55,6 MB em gzip e 558 MB em XML, com 4.456 canais. — https://onetvconnect.com/blog/usa-canada-epg-xmltv
  - O epg.pw serve XML e .xml.gz. — https://epg.pw/xmltv.html?lang=en
- O TiviMate descompacta gzip e zip em memória, e um .gz pode virar mais de 100 MB de XML durante o parse. — https://tvimate.co/tivimate-epg-setup/ (resumo de busca; abrir a página antes de citar de novo)
- Parse em pouca RAM. Expat é um parser de stream: o documento entra em pedaços e cabe qualquer tamanho. `XML_GetBuffer` mais `XML_ParseBuffer` evita a cópia dupla. — https://libexpat.github.io/doc/api/latest/ ; https://www.xml.com/pub/1999/09/expat/index.html
  - O `xmlTextReader` do libxml2 também é forward-only e de baixo consumo, mas é mais lento. Um benchmark antigo mediu 2x a 5x. — https://mail.gnome.org/archives/xml/2007-October/msg00029.html ; https://gnome.pages.gitlab.gnome.org/libxml2/devhelp/libxml2-xmlreader.html
  - yxml: só pesquisei por nome, não achei fonte para citar.
- Kodi pvr.iptvsimple:
  - Aceita XMLTV em Gzip e XZ. Tem cache local opcional dos arquivos remotos. Desde o Kodi 20 aceita vários pares M3U/XML. — https://github.com/kodi-pvr/pvr.iptvsimple/blob/Omega/README.md
  - O ajuste global de EPG vai de -12 a +14 horas. `tvg-shift` vale no cabeçalho `#EXTM3U` ou por `#EXTINF`. — https://github.com/kodi-pvr/pvr.iptvsimple ; https://kodi.wiki/view/Add-on:PVR_IPTV_Simple_Client
  - O `tvg-shift` por canal tem relato de que não funciona (issue #932, aberta). — https://github.com/kodi-pvr/pvr.iptvsimple/issues/932
  - Existe a opção "Ignore Case for EPG Channel IDs". — https://github.com/kodi-pvr/pvr.iptvsimple
- TiviMate e intervalo de atualização:
  - Guias sugerem 4 a 6 h, e dizem para não ir abaixo de 2 h por risco de bloqueio de IP. Alguns recomendam 12 a 24 h. — https://tvimate.co/tivimate-epg-setup/ ; https://tivimateplayer.co.uk/tivimate-epg-not-updating-fixes/
  - O intervalo de atualização da playlist vai de 1 a 24 h. — https://watchingiptv.com/how-to-update-iptv-playlist-and-epg/
  - Uma vez a cada 24 h basta. A fonte tem 7 a 14 dias de grade, e mudanças do mesmo dia não aparecem. — https://ott-tv.org/blog/xmltv-epg-explained/
  - O TiviMate Premium mescla 2 ou 3 fontes, e a primeira fonte vence. — https://ott-tv.org/blog/xmltv-epg-explained/
  - O OTT Navigator não teve fonte achada nesta pesquisa.

## 2. M3U estendido

- O arquivo começa com `#EXTM3U`. Cada canal é uma linha `#EXTINF:-1` com atributos entre aspas, seguida da URL. — https://uniplayer.net/docs/specs/m3u/ ; https://en.wikipedia.org/wiki/M3U
- Atributos:
  - `tvg-id` liga ao `id` do canal no XMLTV.
  - `tvg-name` casa com o `display-name` e serve de fallback.
  - `tvg-logo` é a URL do logo.
  - `group-title` agrupa. No Kodi pode ter vários grupos separados por `;`.
  - `tvg-chno` é o número do canal.
  - Fontes: https://github.com/kodi-pvr/pvr.iptvsimple/blob/Omega/README.md ; https://uniplayer.net/docs/specs/m3u/
- Cabeçalho `url-tvg` ou `x-tvg-url` aponta o XMLTV. — https://uniplayer.net/docs/specs/m3u/ ; https://iptv.us.com/blog/guide-to-m3u-playlists-iptv
- Catchup:
  - `catchup` (alias `catchup-type`) define o modo: default, append, shift, flussonic, xc.
  - `catchup-source` é o template da URL. `catchup-days` é a profundidade em dias. `catchup-correction` é o ajuste de horas.
  - O Kodi aceita `catchup`, `catchup-source`, `catchup-days` e `catchup-correction`.
  - Fontes: https://uniplayer.net/docs/specs/m3u/ ; https://github.com/kodi-pvr/pvr.iptvsimple/blob/Omega/README.md
- Armadilha: Flussonic usa timestamp UTC, e o Xtream Codes espera hora local do relógio do painel. — https://uniplayer.net/docs/specs/m3u/
- Cabeçalhos HTTP por canal: `URL|user-agent=X&referer=Y`, `#EXTVLCOPT` (`http-user-agent`, `http-referrer`) e `#EXTHTTP` (JSON). — https://uniplayer.net/docs/specs/m3u/
- Convenção do iptv-org: `tvg-id` no formato `canal.pais`, por exemplo `bbc.uk` ou `cnn.us`. — https://iptv.us.com/blog/guide-to-m3u-playlists-iptv (resumo de busca) ; índice em https://iptv-org.github.io/iptv/index.m3u
- A spec "oficial" é informal: a Wikipedia e as docs do Kodi são as referências práticas. Não achei RFC para `tvg-*`.

## 3. Xtream Codes API

- Endpoint: `http://HOST:PORT/player_api.php?username=U&password=P`. A ação vai em `&action=`. — https://github.com/worldofiptvcom/xtream-codes-api-documentation ; https://xtreamiptv.codes/xtream-codes/api/
- Ações vistas: `get_live_categories`, `get_live_streams`, `get_vod_streams`, `get_series`, `get_short_epg&stream_id=X[&limit=N]`. — https://github.com/worldofiptvcom/xtream-codes-api-documentation ; https://github.com/chazlarson/py-xtream-codes/blob/master/xtream.py
  - `get_simple_data_table`: o nome apareceu na minha lista de pesquisa, mas nenhuma fonte o documentou. **[não verificado]**
- EPG completo vem de `http://HOST:PORT/xmltv.php?username=U&password=P`. — https://iptvscene.com/tools/xtream-guide.php
- Sem `limit`, `get_short_epg` devolve os próximos 4 itens. — https://pkg.go.dev/github.com/tellytv/go.xtream-codes
- `title` e `description` em `epg_listings` vêm em base64. Há campos `start_timestamp` e `stop_timestamp`, além de `start` e `end` como texto. — https://pkg.go.dev/github.com/tellytv/go.xtream-codes ; https://pkg.go.dev/github.com/ze3ma0/yptv
- URL de stream ao vivo: `/live/{user}/{pass}/{stream_id}.{ts|m3u8}`. — https://github.com/worldofiptvcom/xtream-codes-api-documentation ; https://xtreamiptv.codes/xtream-codes/api/
  - O `get.php` aceita `type=m3u_plus&output=ts|hls`. — https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API
- `user_info` traz `auth`, `status`, `exp_date` (Unix), `max_connections`, `active_cons` e `allowed_output_formats` (por exemplo `m3u8` e `ts`). — https://xtreamiptv.codes/xtream-codes/api/ ; https://pkg.go.dev/github.com/kludgarr/go.xtream-codes
- `server_info` traz `url`, `port`, `https_port`, `server_protocol`, `timezone`, `timestamp_now` e `time_now`. — https://pkg.go.dev/github.com/sheghun/go.xtream-codes ; https://servextex.github.io/xtream-ui-ubuntu/player_api_documentacion.html
- Cada stream live tem `stream_id`, `epg_channel_id`, `stream_icon` e `tv_archive`. `tv_archive_duration` é a janela de catchup em dias. — https://xtreamiptv.codes/xtream-codes/api/ ; https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API ; https://deepwiki.com/northernpowerhouse/pvr.dispatcharr/6.1-xtream-codes-api
- Timeshift: `http(s)://host:port/timeshift/USER/PASS/DURACAO_MIN/YYYY-MM-DD:HH-MM/STREAM_ID.ext`. A duração é em minutos e o horário de início fica na hora local do painel. — https://github.com/worldofiptvcom/xtream-codes-api-documentation ; https://uniplayer.net/docs/specs/m3u/ ; https://iptv.community/threads/xtreamcodes-api-catchup.20248/
  - Existe `epg_shift` opcional, de -12 a 12 horas. — https://github.com/zaclimon/xipl/wiki/Xtream-Codes-API
- Falhas comuns:
  - 401/403 vêm de credencial errada, conta bloqueada ou User-Agent recusado. Trocar para `VLC` ou `IPTVSmartersPro` costuma resolver. — https://lumiptv.com/blog/xtream-codes-connection-failed-fix ; https://freeiptvcheck.com/blog/iptv-not-working-fix
  - Limite de conexões: duas aberturas seguidas contra uma conta de 1 conexão geram rejeição ou bloqueio. — https://github.com/jellyfin/jellyfin/issues/18213
  - A URL embute usuário e senha, então nunca registrar em log. — https://xtreamiptv.codes/xtream-codes/api/
  - O código HTTP 884 e os redirects para URL com token não tiveram fonte própria. **[não verificado]**
  - Painéis só em HTTP: a spec expõe `https_port`, mas não achei nada sobre painéis sem HTTPS. **[não verificado]**

## 4. Casamento de EPG

- O Kodi casa em 3 passadas. Primeiro `tvg-id` com o `id` do XMLTV. Depois `tvg-name` (espaço vira `_`) com o `display-name`. Por fim o nome do canal. O primeiro acerto encerra a busca. — https://github.com/kodi-pvr/pvr.iptvsimple/blob/Omega/README.md
- O casamento por id é exato e diferencia maiúsculas: `ESPN.us` não é `espn.us`. É a causa nº 1 de "sem EPG". — https://ott-tv.org/blog/xmltv-epg-explained/
- O projeto iptv-org/epg baixa a grade de dezenas de sites e gera `guide.xml`, ou `guide.xml.gz` com `--gzip`. Usa `xmltv_id` vindo do iptv-org/database. O timeout padrão é 30000 ms e a concorrência padrão é 1. — https://github.com/iptv-org/epg/blob/master/README.md
  - Existe fonte epgshare01 dentro do projeto. — https://github.com/iptv-org/epg/blob/master/sites/epgshare01.online/readme.md
- Fuzzy por nome normalizado (tirar HD, FHD, 4K, prefixo de país): não achei fonte publicada com regra pronta. É técnica de projeto, só o princípio está sustentado pelo fallback por nome do Kodi.
- Mescla de vários XMLTV: o TiviMate usa "primeira fonte vence". — https://ott-tv.org/blog/xmltv-epg-explained/ O NuvioTV (fork) mescla vários guias em um só. — https://github.com/killamfkr/NuvioTV-live

## 5. Stremio e Nuvio

- Stremio não tem recurso EPG separado: usa catalog, meta e stream. — https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/epg.md ; https://stremio.github.io/stremio-addon-sdk/epg.html
- No manifest: `types: ["tv"]` e `behaviorHints.epgProvider: true`, só quando há grade de verdade. — mesmas fontes
- O catálogo de guia declara os extras `skip` e `date`. Sem `date` devolve `{ metas }` (canais com `isLive: true`). Com `date` devolve `{ metasDetailed }`. — mesmas fontes
- Cada programa é um item de `meta.videos` com `startTime` e `endTime` em ISO 8601, com `endTime` maior que `startTime`. As linhas do guia levam `hasScheduledVideos: true`. Não apontar `defaultVideoId` para um programa. O stream é pedido pelo id do canal. — mesmas fontes
- Cache sugerido: 300 s de max-age, 1800 s de stale-revalidate e 604800 s de stale-if-error. — https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/epg.md
- Os PRs do SDK que trazem isso são https://github.com/Stremio/stremio-addon-sdk/pull/391 e https://github.com/stremio-community/stremio-addon-sdk/pull/56. O primeiro pode não estar na versão publicada do SDK. **[não verificado]**
- Addons populares:
  - USA TV (drykilllogic), o mais instalado. — https://www.firesticktvstream.com/usa-tv-stremio-addon-guide/
  - TvVoo, que lista canais VAVOO e resolve HLS. — https://stremio-addons.net/addons/tvvoo
  - US Live TV, estático, a partir do iptv-org. — https://github.com/notr3kt/stremio-us-live-tv
  - Lista geral: https://addons-stremio.com/stremio-live-tv-addons
- Nuvio:
  - O NuvioTV oficial (NuvioMedia/NuvioTV) tem um pedido aberto de aba Live TV com IPTV, EPG, favoritos e addons de TV, sem prazo. — https://github.com/NuvioMedia/NuvioTV/issues/2619
  - Um artigo diz que o Nuvio toca live via addons, mas não tem importação M3U/Xtream nem EPG nativos. Cada plataforma tem código e recursos próprios. — https://nuviosync.com/blog/nuvio-live-tv
  - Existe um fork "Nuvio + IPTV" (homelessbrian) e outro (killamfkr/NuvioTV-live). O fork aceita várias playlists M3U e vários XMLTV mesclados, login Xtream, grade EPG com toque, catchup, lembretes, controle parental, e pausar/retroceder o ao vivo gravando no aparelho. — https://github.com/homelessbrian/NuvioTV/releases/tag/1.1.0-beta.4.7 ; https://github.com/killamfkr/NuvioTV-live
  - O fix de 403 por User-Agent em DASH apareceu na 0.8.12-beta. — https://nuviosync.com/blog/nuvio-live-tv
  - O repositório tapframe/NuvioTV que eu procurava não apareceu. Os repositórios achados são NuvioMedia/NuvioTV e forks.

## 6. Reprodução

- webOS:
  - HLS é o protocolo principal. `.ts` é aceito com H.264, HEVC e MPEG-2 (3.5) e com H.264/HEVC mais AC3/E-AC3/AAC (6.0). — https://webostv.developer.lge.com/develop/specifications/streaming-protocol-drm ; https://webostv.developer.lge.com/develop/specifications/video-audio-35 ; https://webostv.developer.lge.com/develop/specifications/video-audio-60
  - Live seek existe desde o webOS 3.0. Avanço e retrocesso rápidos e velocidade acima de 1.0x não são aceitos. HTTP/2 só a partir do 5.0. — https://webostv.developer.lge.com/develop/specifications/streaming-protocol-drm
  - Tags HLS não suportadas: `EXT-X-PROGRAM-DATE-TIME`, `EXT-X-ALLOW-CACHE`, `EXT-X-DISCONTINUITY-SEQUENCE`. `EXT-X-DISCONTINUITY` só vale para descontinuidade de PTS a partir do 4.0. — mesma fonte
  - A página do webOS descreve o player HTML5/mediaOption. Para o nosso caso, com pipeline nativo em C, não achei fonte do pipeline interno. **[não verificado]**
- Tizen AVPlay:
  - `setBufferingParam(PLAYER_BUFFER_FOR_PLAY|RESUME, SECOND, n)` exige no mínimo 4 s. Exemplo oficial usa 5 s para início e 15 s para retomada. — https://developer.samsung.com/smarttv/develop/api-references/samsung-product-api-references/avplay-api.html ; https://developer.samsung.com/smarttv/develop/guides/multimedia/media-playback/using-avplay.html
  - `setTimeoutForBuffering` tem padrão de 20 s. O buffer padrão é 32 MB ou 10 s. — AVPlay API
  - `setStreamingProperty` aceita `PREBUFFER_MODE`, `ADAPTIVE_INFO` e `USER_AGENT`. As chamadas de buffering exigem estado IDLE. — AVPlay API
  - Não achei fonte direta sobre TS ao vivo cru no AVPlay. **[não verificado]**
- Android Media3:
  - `HlsMediaSource` cobre live comum e LL-HLS. TS cru é suportado como container progressivo (`ProgressiveMediaSource`). — https://developer.android.com/media/media3/exoplayer/supported-formats ; https://developer.android.com/media/media3/exoplayer/hls
  - Em `BehindLiveWindowException` (`ERROR_CODE_BEHIND_LIVE_WINDOW`): `seekToDefaultPosition()` e `prepare()` de novo. — https://developer.android.com/media/media3/exoplayer/live-streaming
  - Janela de live com pelo menos 1 minuto é o recomendado. — https://medium.com/google-exoplayer/hls-playback-in-exoplayer-a33959a47be7 (resumo de busca)
  - `LiveConfiguration` tem `targetOffsetMs`, `minOffsetMs`, `maxOffsetMs`, mais ajuste automático de velocidade. Desligar fixando min e max em 1.0f. — https://developer.android.com/media/media3/exoplayer/live-streaming
- FFmpeg/libmpv:
  - `probesize` padrão 5.000.000 bytes, `analyzeduration` padrão 5.000.000 µs, `fflags=nobuffer`. — https://ffmpeg.org/ffmpeg-formats.html
  - mpegts: `max_packet_size` 204800, `resync_size` 65536, `scan_all_pmts` -1. — mesma fonte
  - HTTP: `reconnect`, `reconnect_streamed`, `reconnect_at_eof`, `reconnect_delay_max` (segundos), `reconnect_max_retries`, `rw_timeout` (µs), `user_agent` (padrão `Lavf/<versão>`), `multiple_requests` (-1 auto). — https://ffmpeg.org/ffmpeg-protocols.html
  - mpv: `profile=low-latency`, `--demuxer-readahead-secs`, `--demuxer-max-bytes`, `--demuxer-lavf-probesize`, `--untimed`. A maior parte da latência vem do timing, não do buffer. — https://mpv.io/manual/stable/
- Zapping:
  - Pré-juntar canais adjacentes elimina o tempo de troca, ao custo de banda. Em multicast, mandar o canal anterior e o próximo por 1 minuto depois da troca zera a espera em quase metade das trocas. — https://ieeexplore.ieee.org/document/1293012/ ; https://www.researchgate.net/publication/220307585_Reducing_channel_change_delay_in_IPTV_by_predictive_pre-joining_of_TV_channels
  - Relógios de áudio e vídeo independentes reduzem até 62% do tempo total de troca. — https://www.researchgate.net/publication/221170171_A_Novel_Scheme_for_a_Fast_Channel_Change_in_Multicast_IPTV_System
  - Aviso: esses trabalhos são sobre multicast. Em HTTP unicast com limite de conexões do provedor, pré-abrir canais pode estourar `max_connections` (ver seção 3). Isso é inferência minha.

## 7. Fuso horário

- Sem `TZ`, a libc usa `/etc/localtime`. `TZ` vazio ou inválido resulta em UTC. — https://sourceware.org/glibc/manual/latest/html_node/TZ-Variable.html ; https://www.man7.org/linux/man-pages/man3/tzset.3p.html
- `mktime` com `tm_isdst = -1` deixa a libc decidir o horário de verão. — https://linux.die.net/man/3/mktime
- XMLTV traz offset explícito, então dá para converter tudo para epoch UTC no parse e só formatar na tela. — https://uniplayer.net/docs/specs/xml/
- O Xtream expõe `timestamp_now`, `time_now` e `timezone` no `server_info`, úteis para calibrar o relógio do painel. — https://pkg.go.dev/github.com/sheghun/go.xtream-codes
- Timeshift usa hora local do painel e XMLTV usa offset próprio: guardar epoch e converter na borda. — https://uniplayer.net/docs/specs/m3u/
- Se a TV tem `TZ` não configurado, nem os docs do webOS nem do Tizen que achei dizem como obter o offset do usuário pela API nativa. **[não verificado]**

## Recomendações técnicas

Números abaixo são propostas de projeto, cada uma ancorada na fonte indicada.

1. Parse de XMLTV em stream com expat, `XML_GetBuffer` mais `XML_ParseBuffer`, em blocos de 64 KB. Nunca carregar o arquivo inteiro. Descompactado chega a 558 MB. — expat; onetvconnect
2. Descompactar gzip em fluxo (zlib `inflate` com buffer de 64 KB), sem materializar o XML em disco ou RAM. O TiviMate faz em memória e pode passar de 100 MB. — tvimate.co
3. Teto de parse: abortar a 300 MB de XML descompactado ou 5 minutos, com erro claro. Cobre o caso US2 (77 MB) com folga e barra o US_LOCALS1 (558 MB). — onetvconnect
4. Memória do EPG: guardar só canais que casam com a playlist e só a janela de -6 h a +48 h (ou 7 dias, se o usuário pedir). Teto de 8 MB de RAM para o índice, e programas em arquivo mmap. A fonte tem 7 a 14 dias, então descartar o resto no parse. — ott-tv.org
5. Normalizar tudo para epoch UTC no parse. `start`/`stop` em `YYYYMMDDHHMMSS ±HHMM`. Sem offset, assumir UTC e deixar um ajuste manual de -12 a +14 h, igual ao Kodi. — uniplayer; kodi
6. Casamento em 3 passadas como o Kodi: `tvg-id` exato, depois `tvg-name` com `_`, depois o nome. Antes do id exato, tentar id sem diferenciar maiúsculas. A normalização fuzzy (tirar HD/FHD/4K/país) vira quarta passada, e só deve valer com acerto único. — kodi README
7. Mesclar vários guias com "primeira fonte vence", limite de 3 fontes. — ott-tv.org
8. TTL do EPG: atualizar a cada 12 h por padrão, mínimo de 2 h. Fontes comuns mudam 1 vez por dia, e menos de 2 h arrisca bloqueio de IP. Usar `ETag`/`If-Modified-Since` quando houver. — tvimate.co; ott-tv.org
9. Cache de playlist M3U: TTL de 6 h com refresh manual, e no Xtream guardar `get_live_streams` por 6 h. Atualizar em segundo plano e mostrar o cache velho enquanto isso.
10. Xtream: chamar `get_short_epg` com `limit=2` ou `limit=4` só para o canal em foco (padrão de 4 itens), e decodificar `title`/`description` de base64. Para a grade, preferir `xmltv.php` uma vez por TTL em vez de uma chamada por canal. — go.xtream-codes
11. Xtream: ler `user_info.allowed_output_formats` e escolher `m3u8` se existir, senão `ts`. Ler `max_connections` e nunca abrir mais streams que isso. Desabilitar pré-buffer de canal adjacente se `max_connections` for 1. — xtreamiptv.codes; jellyfin#18213
12. Xtream catchup: montar `/timeshift/U/P/DURACAO_MIN/YYYY-MM-DD:HH-MM/ID.ts` em hora local do painel (`server_info.timezone`), só se `tv_archive` for 1, e limitar a `tv_archive_duration` dias. — uniplayer; xipl
13. Rede: timeout de conexão de 10 s e de leitura (`rw_timeout`) de 15 s no live. Para o `xmltv.php`, 60 s e no máximo 1 retry. O padrão do iptv-org/epg é 30 s por requisição. — ffmpeg-protocols; iptv-org/epg
14. Reconexão no libmpv/ffmpeg: `reconnect=1`, `reconnect_streamed=1`, `reconnect_delay_max=5`, `reconnect_max_retries` baixo (3 a 5). Sem isso, queda de TCP no live mata o stream. — ffmpeg-protocols
15. Início rápido no live: `probesize` de 1 a 2 MB e `analyzeduration` de 1 a 2 s (os padrões são 5 MB e 5 s) e `fflags=nobuffer`. Se faltar áudio ou legenda, subir de volta. — ffmpeg-formats; mpv manual
16. User-Agent configurável por playlist, com padrão tipo `VLC`, e suporte a `#EXTVLCOPT`, `#EXTHTTP` e `URL|user-agent=`. 401/403 sem causa clara costuma ser UA. Nunca registrar a URL com credenciais em log. — lumiptv; uniplayer; xtreamiptv.codes
17. Android: preferir `HlsMediaSource` para `.m3u8` e `ProgressiveMediaSource` para `.ts`. Tratar `ERROR_CODE_BEHIND_LIVE_WINDOW` com `seekToDefaultPosition()` mais `prepare()`. Fixar a velocidade de live em 1.0f se o áudio tropeçar. — developer.android.com
18. Tizen AVPlay: buffer de início de 4 a 5 s e de retomada de 8 a 15 s (mínimo aceito é 4 s). Chamar `setBufferingParam` e `setTimeoutForBuffering` em IDLE. Manter `setTimeoutForBuffering` perto dos 20 s padrão. — AVPlay API
19. webOS: dar preferência a HLS; evitar depender de `EXT-X-PROGRAM-DATE-TIME` e `EXT-X-DISCONTINUITY-SEQUENCE`, que não são suportadas. Não prometer velocidade acima de 1.0x no live. — webostv.developer.lge.com
20. Zapping: manter o último frame até chegar o primeiro frame novo. Cancelar o canal anterior só depois de 150 a 300 ms sem nova tecla (debounce) para zapping em sequência. Pré-buffer de adjacentes só em fonte sem limite de conexões. — IEEE 1293012; researchgate (62%)
21. Guardar `tvg-id` e URL original no favorito, não só o índice, porque ids e ordem mudam a cada refresh de playlist. Mostrar logo com cache em disco e placeholder.
22. Não confiar em fuso do sistema da TV: usar epoch UTC internamente e um deslocamento configurável pelo usuário, com padrão do `server_info.timezone` quando for Xtream. — glibc TZ; xtreamiptv.codes
