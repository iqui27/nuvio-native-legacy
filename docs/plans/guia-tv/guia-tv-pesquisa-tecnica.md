# Guia TV (Live TV / EPG) — pesquisa técnica

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
