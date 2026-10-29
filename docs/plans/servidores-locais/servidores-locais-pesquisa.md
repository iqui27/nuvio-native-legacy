# Servidores locais: notas de pesquisa (06/10/2026)

Material de apoio para `servidores-locais-plano.md`.
- **[NÃO VERIFICADO]**: afirmação vista em busca ou em fonte secundária, sem conferência na documentação primária. Confirmar antes de implementar.
- **Código**: verificado em `/Volumes/ExternalSSD/nv-202-amigosdup` (só leitura).

---

## 1. O que muda o jogo (2025–2026)

1. **Jellyfin 12.0, lançado em 07/09/2026.** A numeração pulou de 10.11 para 12.0.
   - Autorização legada **desligada por padrão**. Deixaram de funcionar: `api_key` na query, `X-Emby-Token`, `X-MediaBrowser-Token`, `X-Emby-Authorization`.
   - Prefixos `/emby/` e `/mediabrowser/` removidos.
   - `GET /QuickConnect/Initiate` removido (só POST).
   - .NET 10 e Swashbuckle v10: os SDKs precisam ser regenerados.
   - Fontes: https://jellyfin.org/posts/jellyfin-release-12.0/ · https://gist.github.com/nielsvanvelzen/ea047d9028f676185832e51ffaf12a6f · PR que liga/desliga: https://github.com/jellyfin/jellyfin/pull/15559 · opção `EnableLegacyAuthorization` (padrão true no 10.11, false no 12): https://github.com/jellyfin/jellyfin/pull/13306
   - Relatos de quebra: https://github.com/music-assistant/support/issues/6369 · https://github.com/dannyvfilms/Floppy/issues/1135 · https://github.com/sahara101/Movie-Roulette/issues/91
   - **Impacto no nosso código:** `src/jellyfin.c` `anexarApiKey()` acrescenta `api_key=` às URLs do player, o que deve dar 401 no 12.x. O cabeçalho `Authorization: MediaBrowser … Token=` já está certo. O `ApiKey=` (com maiúscula) continua aceito "quando cabeçalho não é possível" (gist). **[NÃO VERIFICADO]**: se todas as rotas de stream, HLS e legenda aceitam `ApiKey` no 12.x.
   - Versão 12.2 já saiu: https://jellywatch.app/blog/jellyfin-12-2-release-changelog-what-changed-should-you-upgrade-2026
2. **NuvioTV (oficial, Android TV) tem PR em rascunho com Jellyfin + Emby.** É o #3728, aberto e não mesclado em 03/10/2026.
   - Traz fileiras na Home, busca, aba Servidores, casamento com títulos dos add-ons como "fonte alternativa", direct play/transcode e progresso.
   - Requisitos: Jellyfin 10.9+ e Emby 4.7+. Sem Plex.
   - https://github.com/NuvioMedia/NuvioTV/pull/3728
3. **Plex cobra o acesso remoto também em apps de terceiros.**
   - Desde 29–30/04/2025, assistir mídia pessoal fora de casa exige Plex Pass ou Remote Watch Pass (US$ 1,99/mês ou US$ 19,99/ano), ou que o administrador do servidor tenha Plex Pass.
   - A cobrança passou a valer em TVs e, por volta de out/2026, em apps de terceiros (o Infuse foi avisado). Em casa (LAN) nada muda.
   - https://www.howtogeek.com/plex-is-now-enforcing-remote-play-restrictions-on-tvs/ · https://www.xda-developers.com/plex-just-blocked-one-of-the-last-free-ways-to-stream-your-own-movies-remotely/ · https://9to5mac.com/2025/11/27/plex-paywall-for-remote-streaming-now-being-enforced/ · https://www.trustedreviews.com/news/plex-is-making-changes-thatll-affect-a-lot-of-streamers
   - O mecanismo (qual erro a API devolve) não está documentado publicamente **[NÃO VERIFICADO]**.
4. **Plex: autenticação JWT/JWK e documentação oficial da API.**
   - O PIN agora pode ser feito em `clients.plex.tv/api/v2/pins` com uma JWK Ed25519. O token JWT dura 7 dias e é renovado com a chave do aparelho.
   - Fontes: https://forums.plex.tv/t/jwt-authentication/931646 · https://forums.plex.tv/t/exchanging-pin-for-jwt-token/941120 · https://developer.plex.tv/pms/ · anúncio: https://www.plex.tv/blog/plex-pro-week-25-api-unlocked/
   - O fluxo clássico (`plex.tv/api/v2/pins`, `strong=false`, `authToken`) é o que o `plex.c` usa. Se foi depreciado: **[NÃO VERIFICADO]**.
5. **Jellyfin não tem consulta "achar item por IMDb/TMDB".**
   - Existe `hasTmdbId`; não existe `anyProviderIdEquals`. Clientes montam índice local com `/Items?hasTmdbId=true&fields=ProviderIds`.
   - https://github.com/jellyfin/jellyfin/discussions/10156 · https://github.com/jellyfin/jellyfin/issues/1990 · padrão de índice: https://github.com/ScottCooper92/binge-jellyfin/issues/4
   - O Emby tem `AnyProviderIdEquals`.

---

## 2. Jellyfin: API

### Autenticação
- Cabeçalho: `Authorization: MediaBrowser Client="…", Device="…", DeviceId="…", Version="…", Token="…"`. Vale no 10.x e no 12.x (gist acima).
- Senha: `POST /Users/AuthenticateByName` `{Username, Pw}` → `AccessToken`, `User.Id`.
- **Quick Connect:**
  1. `GET /QuickConnect/Enabled` (bool).
  2. `POST /QuickConnect/Initiate` (cabeçalho sem token) → `Secret`, `Code` de 6 dígitos.
  3. `GET /QuickConnect/Connect?secret=` até `Authenticated: true`. Depois de expirar, 404.
  4. `POST /Users/AuthenticateWithQuickConnect {"Secret"}`.
  - Fontes: https://github.com/navidrome/navidrome/pull/6174 · GET dá 405 no 12: https://github.com/Fathom-Media/fathom/issues/53 · Quick Connect desligado: https://github.com/jellyfin/jellycon/pull/457 · doc oficial: https://jellyfin.org/docs/general/server/quick-connect/
  - O `jellyfin.c` já usa POST no Initiate (correto).

### Rotas (10.9+)
- O 10.9 tirou o `userId` do caminho: `/Users/{id}/Items` virou `/Items?userId=`. https://github.com/jellyfin/jellyfin/releases/tag/v10.9.0 · Infuse cobrado por usar rotas antigas: https://community.firecore.com/t/update-jellyfin-api-use-of-deprecated-paths/54999
- Rotas: `/UserViews`, `/Items` (`fields=ProviderIds,…`), `/UserItems/Resume`, `/Items/Latest`, `/Shows/NextUp`, `/Shows/{id}/Seasons`, `/Shows/{id}/Episodes`. Os parâmetros exatos de Resume/NextUp/Latest **[NÃO VERIFICADO]** na doc primária (o ReDoc em https://api.jellyfin.org não carregou).
- Assistido: `POST`/`DELETE /UserPlayedItems/{id}` → `UserItemDataDto`. https://github.com/mia-cx/pendia/pull/91
- `UserData` (`PlaybackPositionTicks`, `Played`, `LastPlayedDate`). Exige token de usuário: com chave de API o `UserData` some. https://github.com/jellyfin/jellyfin/issues/11408 · bug de `/UserItems/{id}/UserData`: https://github.com/jellyfin/jellyfin/issues/15733
- No 12.0, o `GetItems` aplica `recursive` com filtros só quando há `includeItemTypes` (notas do 12.0).
- Busca por IMDb no `searchTerm` não é confiável: o Jellyfin não usa o IMDb como provedor. https://forum.jellyfin.org/t-search-by-imdb-id-no-longer-returns-results-if-there-s-no-tmdb-entry · a busca ficou mais rápida no 10.11: https://jellyfin.org/posts/jellyfin-release-10.11.0/

### Reprodução
- `POST /Items/{id}/PlaybackInfo?UserId=` com `DeviceProfile` (`MaxStreamingBitrate`, `DirectPlayProfiles`, `TranscodingProfiles`, `CodecProfiles`, `SubtitleProfiles`).
  - A resposta traz `MediaSources[]` (`SupportsDirectPlay`, `SupportsDirectStream`, `TranscodingUrl` → `master.m3u8`, `MediaStreams[]`) e `PlaySessionId`.
  - https://gist.github.com/kylehowells/74f538c766a244a3666319860a937030 · modelo: https://typescript-sdk.jellyfin.org/interfaces/generated-client.DeviceProfile.html · pesquisa 1.8: `docs/plans/media-servers-1.8/README.md` (MediaInfoController)
- **Sempre mandar `MaxStreamingBitrate` e `MaxStaticBitrate`.** Sem eles o servidor limita a ~8 Mbps e transcodifica 4K. https://github.com/gitman-101111/gelatinarm/issues/22 (o `jellyfin.c` já manda 120 Mbps).
- Direct play: `/Videos/{id}/stream?static=true&MediaSourceId=…`.
- Legendas: `SubtitleProfiles.Method` = Embed | External | Encode | Hls. Externa por `DeliveryUrl` `/Videos/{id}/{msid}/Subtitles/{idx}/Stream.vtt|srt`. PGS em player que não desenha = Encode (burn-in).
- Codecs por cliente: https://jellyfin.org/docs/general/clients/codec-support/
- Check-ins:
  - `POST /Sessions/Playing`;
  - `/Sessions/Playing/Progress` (~5–10 s);
  - `/Sessions/Playing/Stopped`.
  - https://typescript-sdk.jellyfin.org/interfaces/generated-client.PlaybackProgressInfo.html · sessão "ressuscitada": https://github.com/jellyfin/jellyfin/issues/16815

### Descoberta
- UDP broadcast porta 7359, texto "who is JellyfinServer?". A resposta é JSON `{Address, Id, Name}`.
- https://jellyfin.org/docs/general/post-install/networking/ · https://deepwiki.com/jellyfin/jellyfin-apiclient-python/5.3-server-discovery · jellyfin-webos: https://deepwiki.com/jellyfin/jellyfin-webos/4.2-discovery-protocol

### Novidades
- 10.11.0 (20/10/2025): migração para EFCore `jellyfin.db`, FFmpeg 7.1, fim do ARM32, tone-mapping DV perfil 5. https://jellyfin.org/posts/jellyfin-release-10.11.0/
- Quebras do 12: https://www.breakingchanges.dev/blog/Jellyfin-12-0-breaking-changes/

## 3. Emby: diferenças
- Mantém `/emby/…` (o Jellyfin 12 removeu). O `jellyfin.c` já trata por tipo (`jf_prefixo`).
- `AnyProviderIdEquals` como filtro (consulta direta por imdb/tmdb). https://github.com/jellyfin/jellyfin/issues/1990
- Descoberta: UDP 7359 com "who is EmbyServer?". https://dev.emby.media/doc/restapi/Locating-the-Server.html
- API e autenticação: https://dev.emby.media/doc/restapi/index.html · https://dev.emby.media/doc/restapi/User-Authentication.html · PlaybackInfo: https://dev.emby.media/reference/RestAPI/MediaInfoService/postItemsByIdPlaybackinfo.html · check-ins: https://dev.emby.media/doc/restapi/Playback-Check-ins.html
- Emby Connect é serviço de autenticação e de endereço, não relay. https://emby.media/support/articles/Connectivity.html · fluxo de PIN para TV: **[NÃO VERIFICADO]**.
- Premiere:
  - A reprodução é grátis nos apps próprios (HTML5, Roku, Apple TV, Android TV/Fire, Samsung, LG). O app Android TV é grátis até 5 TVs por servidor.
  - Outras plataformas precisam de Premiere ou de desbloqueio. O texto não fala de apps de terceiros **[NÃO VERIFICADO]**.
  - https://emby.media/community/blogs/entry/582-new-free-playback-and-increased-device-limits/ · https://emby.media/community/topic/127088-is-emby-premiere-required-emby-for-android-tv/
- Resume/NextUp/Latest no formato antigo (`/Users/{id}/Items/Resume`, `/Shows/NextUp`, `/Users/{id}/Items/Latest`) **[NÃO VERIFICADO]**.

## 4. Plex
- **PIN:**
  - Clássico: `POST plex.tv/api/v2/pins` → código de 4 caracteres para `plex.tv/link`; depois `GET /pins/{id}` → `authToken`. É o que está no `plex.c`.
  - JWT: `clients.plex.tv/api/v2/pins` + JWK Ed25519 → `GET /pins/{id}?deviceJWT=…` (fontes no §1.4).
- **Servidores:** `GET clients.plex.tv/api/v2/resources?includeHttps=1&includeRelay=1` → `connections[]` (local, remoto, relay, `*.plex.direct`) **[NÃO VERIFICADO nesta sessão; padrão conhecido]**.
- **Relay:** limite de 2 Mbps (Plex Pass/Remote Watch Pass) ou 1 Mbps (grátis); força transcode; criptografado ponta a ponta. https://support.plex.tv/articles/216766168-accessing-a-server-through-relay/ · https://forums.plex.tv/t/remote-access-bandwidth-limit/586227 · solução de acesso remoto: https://support.plex.tv/articles/200931138-troubleshooting-remote-access/
- **Biblioteca e casamento:** `/library/sections`, `/library/sections/{id}/all?includeGuids=1`. `Guid[]` com `imdb://`, `tmdb://`, `tvdb://` (só nos agentes novos Plex Movie/Plex TV Series). Recomendação: dicionário local guid→item.
  - https://www.plexopedia.com/plex-media-server/api/library/movies/ · https://python-plexapi.readthedocs.io/en/latest/modules/library.html
  - Filtro `?guid=` no servidor **[NÃO VERIFICADO]**.
- **Hubs, decision, timeline, scrobble [NÃO VERIFICADO nesta sessão; conferir em developer.plex.tv/pms]:**
  - `/hubs/home/continueWatching`, `/library/onDeck`, `/hubs/search?query=`;
  - `/video/:/transcode/universal/decision` e `start.m3u8` (`protocol=hls`, `directPlay`, `directStream`, `X-Plex-Client-Profile-Extra`);
  - `/:/timeline?ratingKey=&key=&state=&time=&duration=`;
  - `/:/scrobble?key=&identifier=com.plexapp.plugins.library` e `/:/unscrobble`.
  - https://python-plexapi.readthedocs.io/en/latest/modules/video.html
- **GDM:** `M-SEARCH * HTTP/1.0` para multicast `239.0.0.250:32414`, portas 32410/32412/32413/32414. https://python-plexapi.readthedocs.io/en/latest/_modules/plexapi/gdm.html · https://support.plex.tv/articles/201543147-what-network-ports-do-i-need-to-allow-through-my-firewall/
- **No nosso `plex.c` hoje:**
  - não chama `decision`: oferece o Part como Direct play sempre;
  - `start.m3u8` com parâmetros fixos (`maxVideoBitrate=20000`);
  - token em URL de pôster;
  - índice com imdb e tmdb, mas sem tvdb.

## 5. Capacidades por plataforma
- **LG webOS:** especificação por versão.
  - 1.0: https://webostv.developer.lge.com/develop/specifications/video-audio-10
  - 3.5: https://webostv.developer.lge.com/develop/specifications/video-audio-35
  - 4.5: https://webostv.developer.lge.com/develop/specifications/video-audio-45
  - 6.0: https://webostv.developer.lge.com/develop/specifications/video-audio-60
  - 23: https://webostv.developer.lge.com/develop/specifications/video-audio-230
  - 24: https://webostv.developer.lge.com/develop/specifications/video-audio-240
  - 25: https://webostv.developer.lge.com/develop/specifications/video-audio-250
  - MKV: H.264, HEVC, VP8/9, AV1 (6.0+); áudio DD, DD+, AAC, PCM, DTS "em modelos específicos". Do 23 em diante, DTS-HD e DTS:X em modelos específicos.
  - O perfil do jellyfin-web desliga DTS no webOS 5–22, liga AV1 a partir do 5.0 e limita FLAC a 2 canais. https://deepwiki.com/jellyfin/jellyfin-web/5.1-device-profile-system · https://github.com/jellyfin/jellyfin-web/pull/2942
  - O jellyfin-webos é um invólucro do perfil web. https://deepwiki.com/jellyfin/jellyfin-webos
  - DV em MKV: problema conhecido no webOS. Nota interna no NuvioWeb `LEGACY-WEBOS.md:35`, que cita o mesmo limite no app Jellyfin da LG.
- **Samsung Tizen:** especificação por ano.
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2018-tv-video-specifications.html
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2020-tv-video-specifications.html
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2021-tv-video-specifications.html
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2022-tv-video-specifications.html
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2024-tv-video-specifications.html
  - https://developer.samsung.com/smarttv/develop/specifications/media-specifications/2025-tv-video-specifications.html
  - **DTS não é suportado desde 2018** (e também em 2022 e 2024). TrueHD não aparece nas listas de 2020/2021. AVPlay: https://developer.samsung.com/smarttv/develop/api-references/samsung-product-api-references/avplay-api.html
  - PGS no AVPlay **[NÃO VERIFICADO]**: tratar como Encode.
- **Android TV (Media3):** passthrough de TrueHD/DTS por `AudioCapabilities`; ASS/PGS por extensões **[NÃO VERIFICADO]**. O PR do NuvioTV mantém HEVC/Dolby no transcode e faz burn-in de legenda em imagem (#3728).
- **Perfil atual do nosso código** (`jf_perfil_dispositivo`): conservador e sem prova em TV, conforme `jellyfin.h`.

## 6. TLS em TVs antigas
- O DST Root CA X3 (Let's Encrypt) expirou em 30/09/2021. A ISRG Root X1 só é confiável a partir do webOS 5.0, e a LG não atualiza as versões antigas. Tizen antigo também falha.
- https://community.xibo.org.uk/t/dst-x3-root-expiration-letsencrypt-webos-and-tizen/25971 · https://www.lgwebos.com/topic/7866-lg-webos-4.0-and-below-letsencrypt-root-certificated-expired/ · https://scotthelme.co.uk/lets-encrypt-old-root-expiration/
- O `rede.c` já aceita CA própria (`OPT_CAINFO`, usado para o Discord), e dá para incluir a ISRG Root X1 no curl. O player do sistema não usa isso, então na LAN o preferido é http.
- A CA do `plex.direct` **[NÃO VERIFICADO]**.

## 7. Ecossistema e comunidade

### Pontes Stremio ↔ servidor (o que usuários do Nuvio usam hoje)
- Plex: **Plexio** https://github.com/vanchaxy/plexio (fork citado: https://github.com/natedogg058/plexio). Comparação do Nuvio com Plex/Jellyfin: https://nuviosync.com/blog/nuvio-vs-plex-jellyfin
- Emby: **StreamBridge** (só resolve stream) https://github.com/h4harsimran/streambridge
- Jellyfin:
  - https://github.com/akarazniewicz/stremio-jellyfin (extensão para busca por IMDb)
  - https://github.com/AcrimoniousMirth/stremio-jellyfin
  - **Jellio+** https://github.com/InfiniteAvenger/jellio-plus
  - https://github.com/dm-imdy/stremio-jellyfin-bridge
- Sentido inverso (add-ons Stremio dentro do Jellyfin): Gelato https://github.com/lostb1t/Gelato · Siphon https://github.com/moodiness/jellyfin-plugin-siphon

### Nuvio oficial (org NuvioMedia)
- **NuvioTV:**
  - #2617 integração nativa Jellyfin/Emby: https://github.com/NuvioMedia/NuvioTV/issues/2617
  - #1913 Plex como provedor, login por QR, "substituir o debrid": https://github.com/NuvioMedia/NuvioTV/issues/1913 (duplicado em #2217: https://github.com/NuvioMedia/NuvioTV/issues/2217)
  - #3003 selo de disponibilidade por episódio: https://github.com/NuvioMedia/NuvioTV/issues/3003
  - #3175 "sem fonte" fica em cache depois de adicionar no Plex (lição: não guardar negativo em cache): https://github.com/NuvioMedia/NuvioTV/issues/3175
  - PR #3728 (rascunho) Jellyfin/Emby: https://github.com/NuvioMedia/NuvioTV/pull/3728
- **NuvioMobile:**
  - #1537 Emby (pede o catálogo curado do servidor e arte no hero): https://github.com/NuvioMedia/NuvioMobile/issues/1537
  - #1035 https://github.com/NuvioMedia/NuvioMobile/issues/1035
  - #938 https://github.com/NuvioMedia/NuvioMobile/issues/938
- **NuvioWeb 0.3.38-beta (local):** nenhuma integração com servidor.
  - "media server" ali é o ajudante de Range do próprio app.
  - Fontes ordenadas por grupo de origem: debrid direto → add-ons na ordem do usuário → plugins (`js/core/streams/streamOrdering.js`). Não há "fixar fonte em primeiro".
  - A busca consulta todo catálogo de add-on com `search` (`searchCatalogTargets.js`).
- **Forks:** nenhum fork do Nuvio com Plex/Jellyfin encontrado. Único resultado: https://github.com/levifranzen/nuvio-jellyfin-plugin (★0, não inspecionado). Corby7: só assets, https://github.com/Corby7/nuvio-assets

### Nosso repositório (iqui27/nuvio-native-legacy)
- **#249:** usuário na 2.0.0 com Jellyfin próprio numa LG pede "mostrar o Jellyfin primeiro na lista de fontes" e pergunta sobre Atmos/eARC no 4K vindo da fileira do Jellyfin. **É a origem da decisão D2.**
- **#279:** match framerate; o cliente Jellyfin da Tizen tem o mesmo pedido em aberto. Só contexto.
- **#135:** VIDAA. Só contexto.
- Não há outros pedidos dedicados a Plex/Jellyfin/Emby.

### Clientes elogiados (padrões de UX) [NÃO VERIFICADO: sem threads lidos]
- Infuse (biblioteca única entre servidores, seletor de versão), Swiftfin, Findroid, Streamyfin, Wholphin, Moonfin (https://github.com/Moonfin-Client/Smart-TV, Jellyfin para Tizen/webOS), Plezy (citado como forma de assistir Plex remoto), VidHub.
- Padrões recorrentes: direct play primeiro, transcode só quando necessário; Next Up/Continuar sincronizados com o servidor; uma biblioteca juntando servidores.
- Infuse perdeu o remoto grátis do Plex: https://windowsforum.com/news/plex-blocks-infuse-remote-streaming-without-a-pass-free-windows-vpn-alternatives.446615/
- Kodi (Jellyfin for Kodi, PlexKodiConnect) e Omni/Fusion/Fen/Umbrella: **não pesquisados**.

### Trakt
- Scrobble repetido do mesmo item em janela curta devolve 409 **[NÃO VERIFICADO nesta sessão; documentado na API do Trakt]**.

---

## 8. Estado do código (verificado)
- `src/jellyfin.c/.h`, `src/plex.c/.h`, `src/servidores.c/.h`, `src/plexaddons.c`, `src/jfid.h`; testes `tests/{jellyfin,emby,plex}.c` + `*_server.py`.
- Commits: `ab7414cb`, `14d75186`, `ee862a43`, `443bd816`, `26616838`.
- Ganchos:
  - `main.c:1174` (`plex_ligar_aos_addons`);
  - `app.c:2399` (`servidores_passo`);
  - `perfis.c:381`; `sync.c:2008`;
  - `descoberta.c:2231, 4363-4390, 6294-6307`;
  - `addons.c:463-482, 1582, 1614, 2317-2336`;
  - `catalogo.c:1541`; `progresso.c:276`; `discord.c:564`;
  - `player.c:1511, 1568, 2880, 2883, 2956`;
  - `ajustes.c:1844, 4043-4209, 5881-5898`.
- Lacunas confirmadas:
  - origem extra cheia (`ORIGENS_EXTRA_MAX 2`: plugins + Plex);
  - Jellyfin/Emby sem casamento;
  - sem `badges_detectar` no caminho de origem extra;
  - `pontos()` sem bônus para servidor;
  - sem fileiras Resume/NextUp/Latest;
  - fileiras de servidor marcadas "Catálogo" no editor (`fileiras.c:140-147`) e sem "Ver tudo";
  - busca, biblioteca e selo na ficha não ligados;
  - QR não usado (`qr.c` existe);
  - um interruptor para os três;
  - WGT indisponível.
- Documentos internos: `docs/plans/media-servers-1.8/README.md` (pesquisa 03/10 com fontes oficiais), `STATUS-F11.md`, `docs/releases/2.0.0/NOTAS.md:19`.

## 9. O que falta verificar antes de implementar
1. Se `ApiKey` é aceito em `/Videos/*/stream`, `master.m3u8`, segmentos e `Subtitles` no Jellyfin 12.x.
2. Parâmetros de decision, timeline e scrobble do Plex na doc oficial (developer.plex.tv/pms); se o PIN clássico continua aceito; erro devolvido pelo bloqueio do Remote Watch Pass.
3. Emby: PIN/Connect para TV e posição sobre clientes de terceiros.
4. PGS no AVPlay e no uMediaServer; DTS por modelo LG 23+; DV em MKV na LG.
5. Detecção de plugin Trakt no Jellyfin (`/Plugins`) e se `/UserPlayedItems` dispara o scrobble do plugin.
6. Threads de UX (Reddit) e add-ons Kodi.
