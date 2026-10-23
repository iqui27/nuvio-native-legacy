# Pesquisa: apps de Live TV / IPTV / EPG — o que fazem bem e mal

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
