# Plano de integração com o mercado — AIOStreams e AIOMetadata

**Data:** 12/10/2026. **Worktree:** `/Volumes/ExternalSSD/nv-205-mercado`, branch `agente/205-mercado`.
**Código examinado:** `c0e22824cdf8d0fc28c44258d4ca98a87873469b`. **Gatilho:** issue #466.
**Pedido do dono:** “ver tudo que podemos implementar integrando profundamente com o que há de novo no mercado”.

**Objetivo:** aproveitar contratos documentados para tracking, metadados, coleções, fontes e legendas, sem reimplementar o que o Nuvio já tem.
**Arquitetura proposta:** adaptadores pequenos no núcleo C; captura de identidade/eventos do player, entrega HTTP fora do desenho e projeção dos snapshots externos nas telas existentes. AIOStreams/AIOMetadata continuam responsáveis pelos seus trackers; nosso Worker social não vira gateway de histórico.
**Tecnologia:** C, pthreads, leitores/escritor JSON `js`/`jsw`, HTTP `rede`, arquivos `dados`; pontes uMS/webOS, .NET/Tizen e Media3/Android já existentes.

> Documento de pesquisa e proposta, não implementação nem promessa de release. Nenhum código, configuração, mapa de issues ou fonte offline foi alterado; nenhuma rede, instância, build, teste de aplicação ou commit foi executado. **SUPOSIÇÃO** identifica escolhas nossas, inferências de compatibilidade e estimativas, nunca campos inventados do protocolo. Uma ausência comprovada no parser não prova a causa de um relato em TV.

## 1. Recomendação executiva — ponytail full

1. **Primeiro valor: push watch_state v2 do nosso player.** `start → pause → start → stop`, mais `played/unplayed` manual, com IDs imutáveis nos retries, journal e vínculo explícito perfil→addon. Filmes/séries convencionais primeiro; nenhum tracking de trailer, canal, simulação de PC ou identidade ambígua.
2. **Depois: pull `items` para Continue Watching; em seguida `watched` e `watchlist` por origem.** Não sobrescrever stores locais indiscriminadamente nem transformar importação em gesto que escreve em todos os trackers.
3. **Ganhos paralelos baratos:** hints estruturados de fonte, hash+fileIdx mesmo com URL, `label`/forced/SDH, nota de episódio e arte horizontal do addon. Reutilizar seleção, selos, overlay e ficha existentes.
4. **Integração profunda seguinte:** `meta.collection`, identidade mista IMDb/Kitsu, temporadas especiais e features Jellyfin negociadas. Não refazer séries, coleções da conta, recomendações, Trakt ou Simkl.
5. **Não começar** por OAuth AniList direto, proxy universal de mídia/escrita, servidor Jellyfin nosso, motor novo de sync ou porte de player. Handoff pode atender parte de #467/#438/#459; validar a capacidade efetiva do fornecedor antes de prometer fechamento.

**Alternativas:** (a) adaptador addon direto, recomendado, evita exigir login Jellyfin; (b) usar o servidor virtual Jellyfin do fornecedor, aproveita sua gestão de usuários/histórico, mas hoje nosso cliente tem gaps de bibliotecas/versões; (c) integrar cada tracker diretamente, maior manutenção e autenticação, só se a delegação não atender um requisito concreto. **SUPOSIÇÃO:** o uso direto do HTTP watch_state pelo Nuvio é aceito pelo fornecedor; as referências descrevem principalmente AIOStreams→addon, alimentado por clientes Jellyfin, não uma certificação do nosso cliente.

### Convenções de esforço e impacto

- **S:** parser/adaptação localizada, sem novo store ou navegação; **M:** vários pontos existentes + estado/testes; **L:** identidade/snapshot/navegação/transportes novos ou várias plataformas. Todos são **SUPOSIÇÃO**, não dias nem orçamento.
- **Risco baixo/médio/alto:** probabilidade/impacto funcional, sobretudo falso visto, perda de estado, credencial ou pessoa errada.
- **C:** núcleo compartilhado LG/webOS, Samsung .tpk, Android, PC/Mac; dados/UI também compilam para web. **N:** entrega nativa com HTTP estrito. **W:** .wgt/VIDAA dependem de transporte/persistência web próprios. **J:** conexão Jellyfin; web está atualmente indisponível nesse caminho.
- **Servidor “nenhum novo”** significa não mudar `servidor/recomendacoes`; o addon/servidor externo ainda precisa oferecer a capacidade. Dependências adicionais são indicadas em cada item.

## 2. Fontes e limites da evidência

As oito cópias locais são a especificação deste estudo. Não houve conferência da documentação publicada ou de versão instalada. Navegação de site que sobrou nos textos não é especificação adicional.

| Sigla | Arquivo e trechos úteis |
|---|---|
| O | `.mercado/aiostreams-overview.md:24-35` — extensões opcionais, ignoradas pelo Stremio |
| M | `.mercado/aiostreams-metadata.md:28-120` — meta, catálogo, pessoas, trailers, temporadas, vídeos e IDs |
| C | `.mercado/aiostreams-collections.md:28-94` — bloco, fontes, ordem, legado |
| S | `.mercado/aiostreams-streams.md:28-82` — behaviorHints, identidade torrent e serviços |
| U | `.mercado/aiostreams-subtitles.md:28-63` — idioma, label, forced/SDH e formatos |
| W | `.mercado/aiostreams-watch-state.md:40-454` — v2, push/pull, bulk, viewers, v1 |
| J | `.mercado/aiostreams-jellyfin-extensions.md:28-244` — discovery, features e endpoints |
| A | `.mercado/aiometadata-jellyfin.md:3-340` — servidor virtual, usuários, reprodução, tracking e operação |

As siglas nas seções remetem a esses arquivos; os trechos específicos também são indicados. `.mercado/` já estava **não rastreada** no início. As cópias não foram adicionadas ao git; quem executar o plano precisará desse mesmo pacote offline. O snapshot `docs/issues/mapa.json:1-5` é de 12/10 e declara base `b7c2c1f9`; o checkout examinado tem commits posteriores. Linha citada é a deste checkout.

### Contratos que não podem ser misturados

- Stremio: envelopes `meta`, `metas`, `streams`, `subtitles`; extensões ficam nos itens, salvo os contratos explícitos abaixo. Guardar corpo bruto em cache **não** significa consumir campos extras.
- watch_state: manifesto `watchState`; pull com `version/items/watched/watchlist/ratings`. **Não existe campo `continueWatching`**: a fileira é a projeção de `items`.
- Jellyfin: DTOs próprios, `MediaSources`, `UserData`, ticks; objeto `aiostreams` em respostas específicas. Não equivale ao formato Stremio.
- Conta Nuvio: RPCs `sync_*`, `progress_key`, `p_profile_id` e vídeo sintético `__nuvio_episode__`; não são campos watch_state (`src/syncprog.c:133-185`).
- Social: `/v1/rec` e atividade têm autenticação/modelo próprios. Não são outbox nem tracker de addon (`servidor/recomendacoes/src/index.js:690-783`).

### Divergências que precisam de fixture, não de adivinhação

1. **Threshold:** W diz 90% por padrão (`W:100-119`); AIOMetadata Jellyfin diz 80% e runtime player→arquivo→meta (`A:128`, `A:278`). Nenhum campo de threshold no manifesto watchState está definido. Não criar `watchState.threshold`.
2. **Handoff/v1:** A ainda descreve `playback` para escrita e `watch_state` para leitura (`A:215-226`); W define v2 e alias v1 (`W:446-454`). Negociar o manifesto real, nunca mandar os dois formatos pelo mesmo play.
3. **Viewer:** W exige slug estável, query-only (`W:416-439`); A descreve nomes/aliases e “event carrying viewer” (`A:222`). Implementar query-only v2; confirmar aliases que casam com o slug, sem colocar `viewer` no corpo por inferência.
4. **Tokens AIOMetadata:** A:12 diz Redis/memória e logout após restart; A:36 diz database e sobrevivência a restart. Confirmar versão/configuração externa antes de prometer operação.
5. **Media info bruto:** A:91-93 diz que UA reconhecido libera dados, mas não define o JSON desses dados. `video`, `audio`, `formats`, `dvprofile`, `countryHints`, auth de imagens e URLs de entrega de legendas ficam **SUPOSIÇÃO de contrato** até referência/fixture adicional.
6. **Datas:** `airTime` não informa timezone. `js_ms_iso` usa `timegm` e não aplica offset (`src/js.c:283-299`); não usar essa leitura sem testar offsets para idade/estreia. `endDate` é descrito como data, sem gramática exaustiva.

## 3. O que já existe — não construir de novo

| Fluxo atual | Evidência e limite relevante |
|---|---|
| Manifestos e resources | `src/addons.h:219-265`, `src/addons.c:1547-1603`; catálogo/stream/subtitles/meta; filtros completos de meta. Descoberta entrega o corpo já baixado (`src/descoberta.c:2214-2223`). Nada de watchState hoje. |
| Meta do addon de origem, séries/episódios | `src/descoberta.c:7207-7252`, `src/descoberta.c:7475-7555`, `src/descoberta.c:6581-6652`; ficha, datas, runtime, vídeos; `CatEp.vid` existe. Temporada 0 descartada no Stremio; null/ausente tende a 1. |
| Coleções reais existentes | Conta/Xperience em `src/colecoes.c:606-704`; movie/series/TMDB/Trakt e telas em `src/vertudo.c:339-438`; saga TMDB em `src/extras.c:1226-1293`. Não leem `meta.collection`. |
| Relacionados e recomendações | TMDB/Trakt para filmes **e séries** (`src/extras.c:1063-1098`, `src/detail.c:2080-2084`); social (`src/recomenda.c:3703-3740`). Não há bloco Stremio universal `recommendations` nas fontes. |
| Tracking direto | Player→Trakt start/pause (`src/player.c:3574-3602`), stop/conclusão (`src/player.c:1816-1969`). Simkl lê pausados (`src/simkl.c:449-544`) e escreve histórico (`src/simkl.c:682-729`), não envia pausa parcial nesse player. |
| Progresso Nuvio/local | `src/progresso.c:277-340`, `src/syncprog.c:74-185`, `src/syncprog.c:220-239`; pending, timestamps, ACK de snapshot, perfil. Isso não é outbox do addon. |
| Vistos/desmarcações | `src/visto.c:80-124`, `src/vistoep.c:103-159`, `src/vistonao.h:15-37`; remoto sem data não vence desmarcação. `vistoep` tem mapa global e chave curta (`src/vistoep.c:18-41`). |
| CW reconciliado | União conta/Trakt/Simkl (`src/descoberta.c:3001-3127`), Next Up não atropela pausa sem prova posterior (`src/descoberta.c:2713-2729`), remoções/ocultação (`src/progresso.c:395-578`). |
| Journal e HTTP reutilizáveis como padrão | `src/contapend.c:223-266`, `src/contapend.c:324-341`; `dados` temp+rename (`src/dados.c:504-528`); `RedePedido`, status/Retry-After/jobs (`src/rede.h:12-114`). Journal da conta não tem sessões/retry watch_state. |
| Jellyfin nativo | Login/Quick Connect, bibliotecas, episódios, fontes/check-ins (`src/jellyfin.c:212-350`, `src/jellyfin.c:434-860`). Seis bibliotecas, 24 itens de Home, oito saídas de playback (`src/jellyfin.h:90-111`, `src/jellyfin.h:194-205`); sem negociação das features AIO. |
| Legendas e skip | Recurso+extras (`src/addons.c:1151-1183`, `src/addons.c:1805-1845`), SRT/VTT/ASS por conteúdo (`src/legenda.c:651-692`); flags embutidas no MKV/Android (`src/mkv.c:157-182`, `src/video_android.c:323-360`). IntroDB/AniSkip/capítulos já existem (`src/intro.c:43-129`, `src/intro.c:426-551`). |

**Cerca obrigatória de identidade:** `CatItem.imdb` é ID genérico, não garantia de IMDb; existe origem e TMDB (`src/catalogo.h:123-153`). `deMeta` pode preferir `imdb_id` e perder ID publicado (`src/descoberta.c:1235-1259`). Para obra IMDb, `cat_id_stream` reconstrói `tt:T:E` e não consulta vídeo Kitsu (`src/catalogo.c:1724-1747`). Não usar cortes por `:` de `vistoep`/Trakt para o novo contrato. O leitor de progresso ainda limita contentId a `%23s` apesar do campo de 48 bytes (`src/progresso.c:105-116`, `src/progresso.h:34-47`). Corrigir só os caminhos necessários ao recorte; não iniciar refatoração geral.

## 4. Inventário de capacidades watch_state

### W1 — Manifesto, filtros e adesão

**O que/ganho:** `resources` inclui `watch_state`, raiz `watchState.version=2`, `push.events/bulk`, `pull.items/watched/watchlist/ratings/ttlSeconds`, `viewers`. Usuário escolhe se o addon participa; metades independentes, prefixo pode casar com meta **ou** vídeo (W:40-75,124-140).
**Hoje/gap:** resources em `src/addons.h:219-239` não incluem tracking; `src/addons.c:1547-1603` já oferece padrão de filtro por resource. UI também não mostra ADD_META entre seus usos (`src/addonsui.c:123-132`).
**Esboço:** ampliar a passagem `addons_manifesto_lido`, parser ancorado por objeto; capability inicialmente desconhecida/desligada até manifesto válido. Omitir `push.events` admite eventos de playback, não watchlist/drop/rating; nomes desconhecidos ignorados. Seleção/consentimento explícitos por perfil, push/pull separados. Não duplicar GET do manifesto nem herdar tracking só porque addon está ativo.
**Porte/risco/deps/issues:** **M/médio**, C+N, W com ressalvas; depende de vínculo de pessoa W8. Servidor: addon configurado, nenhum novo. Atende base de **#466**; mostrar meta-only é ganho pequeno independente, não fecha outra issue por inferência.

### W2 — Push de playback e marca manual

**O que/ganho:** POST `{addonBase}/watch_state/push/{type}/{videoId}.json`; `start` começa/retoma, `pause` cria ponto, `stop` termina com `played` verdadeiro/falso. `played/unplayed` são gestos manuais. Corpo: `id,event,scope,at,metaId,videoId,positionMs,durationMs,played,season,episode,ids` (W:77-122).
**Hoje/gap:** há hooks do player e visto, mas Trakt suprime pausas/saídas entre 80–90% (`src/traktscrobble.c:5-31`) e Simkl só recebe visto pelo caminho atual. Não reaproveitar essa política como watch_state.
**Esboço:** capturar snapshot estável do player; start após prova real, pausa de intenção/sistema distinta de buffering/seek, stop antes de destruir pipeline (`src/player.c:1826-1829`). Posição **real**, não `pos=dur` que o fluxo local usa quando conclui (`src/player.c:1907-1929`). At em segundos, posições/duração em ms; duração desconhecida omitida, não zero inventado. Capturar gesto no orquestrador `visto`/menus/episódios, mas sem repetir `played` automático após `stop`. Proteções anti-trailer, canal, fonte errada e seek continuam (`src/player.c:341-353`, `src/player.c:1848-1889`).
**Porte/risco/deps/issues:** **M/alto**, N; W exige transporte; depende W1/W3/W8/W9. Servidor nenhum novo. **#466** primeira entrega; **#459** apenas alternativa delegada, não prova resolução do pedido Simkl direto.

### W3 — Idempotência, fila, retries e reconexão

**O que/ganho:** id igual nos retries, início da reprodução distingue rewatch; não há `progress` periódico nem `unpause`. 200/204 entregues, 401/403 retêm backlog/reconexão, 429/5xx retry com Retry-After, demais 4xx descarte; background, prazo 15 s e backoff descrito 30 s/2 min/10 min/1 h/6 h por aproximadamente um dia (W:118-160).
**Hoje/gap:** `src/trakt.c:2021-2108` tem uma vaga substituível e falha não reentra; não é fila durável. `contapend` tem padrão de journal/ACK, mas é específico da conta. `rede.h:47-114` já expõe transporte adequado, sem retry implícito.
**Esboço:** **SUPOSIÇÃO:** um módulo `src/watchstate.c/.h` especializado, um worker serial por destino/viewer, journal bounded via `dados`, sem pool novo. Materializar ID/corpo antes de enfileirar, conferir gravação antes de iniciar rede, ACK só da revisão enviada, mutex solto no HTTP. Estado de tentativa/retry/configuração é interno, não JSON wire. Native POST direto com `seguir=0`, TLS verificado, prazo 15000 ms, resposta limitada. Falha de transporte retenta; fila cheia/disco cheio são observáveis, nunca derrubam stop silenciosamente. 3xx sem redirecionar escrita: diagnosticar configuração, não ACK. Não reenviar backlog a URL de outra pessoa após reinstalação.
**Porte/risco/deps/issues:** **M/alto**, N, paridade W **L** separada; depende W1/W8. Servidor nenhum novo. Condição necessária para **#466**; durabilidade contra corte físico de energia não é garantida só pelo rename atual, sem fsync (`src/dados.c:504-528`).

### W4 — Pull `items`: Continue Watching

**O que/ganho:** GET `{addonBase}/watch_state/pull.json?since=<token>`; `items` por vídeo, newest-first, posição e/ou percentual e/ou played, duração opcional. `season:null` significa numeração absoluta; vazio durante playback remoto pode ser correto (W:267-332).
**Hoje/gap:** CW tem merge/ocultação, mas `syncprog` exige duração válida para importar (`src/syncprog.c:93-125`) e usa contrato Nuvio. Não preserva todos os casos wire.
**Esboço:** snapshot separado da origem, com `metaId/videoId/type/season/episode/progressPercent/positionMs/durationMs/played/at` quando presentes. Resolver metadados pelo addon de origem; reusar construtor CW, não copiar corpo para syncprog. Preservar registro sem runtime até resolvê-lo; percentual sozinho não inventa ms precisos. Não gatear `items` por version e não apagar progresso local por ausência remota.
**Porte/risco/deps/issues:** **M/alto**, C+N/W; depende W1/W8/W9 e projeção por origem. Servidor precisa pull. **#466** leitura; relacionado a **#451/#404**, sem alegar que adicioná-la conserta bugs de sync já existentes.

### W5 — `watched`, counts e version gate

**O que/ganho:** `watched.movies` são IDs base, `episodes` IDs de vídeo, `counts` watched/total por aliases (total 0 desconhecido); `version` opaco descreve watched. Presente substitui conjunto completo; omitido não muda; vazio limpa só importação do fornecedor. `since` igual evita read caro (W:287-323,384-414).
**Hoje/gap:** `cat_historico_definir_id` e `vistoep_fonte` dão provas, mas não membership completo removível por origem (`src/catalogo.c:405-428`, `src/vistoep.c:103-159`); mapa vistoep global/ID curto.
**Esboço:** guardar membership/version em snapshot por conta+perfil+configuração+viewer; validar integralmente antes de trocar. Projetar filmes/episódios por ID exato; counts é advisory, não gerar episódios fictícios. Não usar hora do pull como data de visto: arrays não têm timestamp individual. Desmarcação local segue `vistonao`; remover da origem A não remove local/B. Escolher um provider de leitura; não unir snapshot parcial após erro de um serviço.
**Porte/risco/deps/issues:** **L/alto**, C+N/W; depende W4/W8/W9 e isolamento do mapa. Servidor pull/version confiáveis. **#466**; base para filtro **#366**, não fecha UI de filtro sozinha.

### W6 — Next Up, dropped e horário de estreia

**O que/ganho:** `watched.nextUp` candidata por série e `airsAt` em segundos; estreia usa o mais tarde entre esse horário e `meta.released`. `watched.dropped` esconde séries de Next Up/Upcoming; ausente conserva, vazio desfaz drops daquela origem (W:315-317,361-382).
**Hoje/gap:** Next Up/agenda/ocultação já existem (`src/descoberta.c:2713-2756`, `src/agenda.c:1244-1255`), mas não esses campos/origem. Data-only não é hora real.
**Esboço:** ler candidato/type/at, reusar resolução do próximo por episódios/assistidos, validar não lançado e manter horário sem reduzi-lo à data. Persistir dropped por origem e version; não confundir “tirar de CW” com drop. **SUPOSIÇÃO:** `airsAt` deve chegar ao filtro de disponibilidade e agenda existentes, não a um novo scheduler.
**Porte/risco/deps/issues:** **M/médio-alto**, C+N/W; depende W5 e datas corretas. Servidor sem novo, pode omitir horário. Relacionado a **#356** já lançado; sem issue exclusiva de drop neste mapa.

### W7 — Watchlist e ratings de usuário, push/pull

**O que/ganho:** `watchlisted/unwatchlisted` sobre meta movie/series; `rated/unrated` em scope movie/series/season/episode, rating 0–10 com decimais. Pull `watchlist/ratings` são listas completas quando presentes, `kind` movie/series desambigua tipos como anime; não gatear automaticamente pela version de watched (W:205-265,333-359).
**Hoje/gap:** Salvos tem hook de gesto (`src/salvos.c:274-282`, `src/app.c:2208-2214`); notas públicas já têm UI (`src/notasfontes.c:17-29`), mas isso não é nota pessoal watch_state. Não há esse import/export de rating do addon.
**Esboço:** ampliar orquestrador de Salvos e guardar origem/override local com echo window; importação não chama callback de gesto. Rating pessoal separado de IMDb/criticRating; episódio usa videoId, temporada usa metaId+season. `unrated` sem rating. Não enviar watchlist/drop/rating sem evento listado. No show que abrange vários animes, retirar anime IDs inadequados no nível show; seasons/episódios carregam entrada correta (W:263-265).
**Porte/risco/deps/issues:** watchlist **M/alto**, rating completo **L/alto**, C+N/W; depende W5/W8/W9 e decisão de UI para rating. Servidor sem novo. **#466** fase posterior; **#406** é passar notas na ficha, não automaticamente pedir avaliação pessoal; **#393/#438/#467** parciais conforme listas delegadas.

### W8 — Viewers, consentimento e pessoa correta

**O que/ganho:** `watchState.viewers:true` permite `?viewer=slug` em push/pull, nunca no corpo. Slug de nome lowercase, caracteres outros em `-`, trim, 28 caracteres e sufixo de colisão; criado uma vez, rename não muda. Ausência significa dono/histórico compartilhado; 404 viewer desconhecido descarta evento e falha só seu pull (W:416-439).
**Hoje/gap:** perfis podem herdar addons do principal (`src/perfis.c:326-345`), mas isso **não autoriza** escrever histórico do dono da URL. `vistoep` não inclui perfil no registro (`src/vistoep.c:18-41`), e `prog_aplicar_remoto` usa perfil ativo; wrapper atual faz cerca (`src/syncprog.c:220-226`).
**Esboço:** binding explícito histórico→configuração/viewer, distinto do perfil de catálogo. Sem viewers, uma instalação escolhida para uma pessoa; perfil secundário não recebe tracking owner automaticamente. Capturar contexto no enqueue; cancelamento/publicação cercados por conta+perfil+configuração+viewer+geração; reset/reconstrução de projeções ao trocar. URL é credencial, viewer não é autenticação; não sincronizar URL/history para social. Para AIOMetadata configurar alias que realmente casa com slug.
**Porte/risco/deps/issues:** **M/alto**, C+N/W; indispensável desde primeiro push, mesmo adiando UI multi-viewer. Servidor precisa viewers se família compartilhar URL. **#466/#467** e prevenção de mistura; não prova correção de **#404**.

### W9 — Dois espaços de ID e numeração

**O que/ganho:** meta IMDb pode publicar vídeos Kitsu; resource casa prefixo de qualquer um, season/episode vêm do metadata, IDs compartilhados são da obra/entrada anime correta, não IDs próprios do episódio (W:124-140; M:97-120).
**Hoje/gap:** `CatEp.vid` existe, mas obra IMDb ignora-o em `cat_id_stream`; season null/0 não é preservada corretamente no Stremio (`src/descoberta.c:6444-6451`, `src/descoberta.c:6583-6607`). Vistoep e parsers por `:` não são generalizáveis.
**Esboço:** preservar ID original de meta, tipo de rota, vídeo original, coordenadas/presença/null e vocabulário `imdb/tmdb/tvdb/mal/anilist/kitsu/anidb/simkl/trakt` já fornecido. **SUPOSIÇÃO:** sidecar de identidade na ficha/sessão/gesto, em vez de inflar todos os CatItem com mapa grande. ID publicado passa na rota, URL-encoded; não reconstruir usando IMDb nem iniciar buscas extras só para scrobble. Sem identidade exata, não emitir evento incorreto; anunciar recorte limitado.
**Porte/risco/deps/issues:** convencional **M/alto**, anime/misto/absoluto completo **L/alto**, C+N/W; servidor addon que mapeia IDs, nenhum novo. **#466/#467**; relacionado **#391** e #372 histórico, sem diagnóstico causal confirmado.

### W10 — Bulk e marcar até um episódio

**O que/ganho:** `push.bulk:true`, `played/unplayed` apenas, scope series/season, path metaId, `videos` realmente mudados, `part/parts`, até 500 vídeos por parte; especiais quando listados. Sem bulk, um evento por vídeo; single events precedem bulk e marcas novas invalidam intenções antigas por vídeo (W:162-203).
**Hoje/gap:** marcação por temporada/episódio existe em `src/episodios.c:312-356`, mas lotes Simkl/conta não são esse contrato; não incluir toda biblioteca do tracker.
**Esboço:** reutilizar cálculo de pares alterados, preservando vídeo/ordem do metadata e season 0. Fila separa single/bulk, retira vídeos com marca mais nova ou stop que concluiu e mantém partes idempotentes. No primeiro slice só marks individuais; gestos enormes precisam limite comunicado, não truncamento silencioso. Jellyfin “played up to” é outro endpoint, J6.
**Porte/risco/deps/issues:** **M/alto**, C+N/W; depende W3/W9, servidor bulk opcional. **#466** expansão; não reabrir **#352** já parcialmente lançado.

### W11 — Drops, undrops e preferências independentes

**O que/ganho:** `dropped/undropped` de série, like/dislike não é rating numérico; retomar série envia undropped depois do start. Fonte que declara ambos pode considerar o próprio start como undrop (W:226-265).
**Hoje/gap:** ocultar CW existente não é dislike remoto (`src/progresso.c:464-578`); não há evento addon nem botão específico comprovado.
**Esboço:** só criar ação de drop se dono quiser; usar corpo de mudança de watchlist com scope series e IDs apropriados. Nova intenção substitui pendente da mesma série, nunca stop; proteger eco de pull. Separar drop/rating e gesto local ocultar.
**Porte/risco/deps/issues:** **M/médio**, C+N/W; depende W6/W7 e UI aprovada; servidor sem novo. **#466** cobertura posterior, nenhuma issue própria identificada.

### W12 — Compatibilidade v1 e controles de operação

**O que/ganho:** v1 `resource/root playback`, events na raiz, POST `/playback/{type}/{id}.json`, sem pull v1. AIOStreams tem `WATCH_STATE_REPORT_ENABLED` e `WATCH_STATE_PULL_ENABLED` independentes; AIOMetadata handoff depende de reporting/handoff configurados (W:441-454; A:215-226).
**Hoje/gap:** nenhum adaptador addon v1/v2. `servidores_reproducao_*` já reporta Jellyfin e pode provocar handoff externo (`src/jellyfin.c:1751-1798`).
**Esboço:** primeiro v2 explícito. Adicionar v1 apenas com manifesto/fixture real, selecionando um write path por configuração; pull v2 independente se declarado. Não enviar addon direto quando sessão já passa por servidor Jellyfin que a reporta. Não deduplicar destinos diferentes automaticamente: manifesto não identifica conta upstream comum a dois addons.
**Porte/risco/deps/issues:** **S–M/médio-alto**, N/W; depende W1/W3 e confirmação de fornecedor. Sem servidor novo. **#466**, compatibilidade AIOMetadata pode ser condição do primeiro release, não pressuposto.

## 5. Metadata e arte — cada campo deve chegar ao consumidor certo

### M1 — IDs e arte de card

- **`ids`:** IDs/links, números aceitos e normalizados como strings; ganho é menos lookup e tracking correto (M:35,110-120). Hoje só ID genérico/TMDB/origem e vid (`src/catalogo.h:123-153`,179-190). **Esboço:** identidade W9, mapa delimitado e snapshots; não trocar ID de rota pelo alias IMDb. **M/alto**, C; deps W9; servidor nenhum novo; **#466/#467**, melhora social sem inventar support custom-ID no backend.
- **`landscapePoster`:** imagem horizontal para CW/Next Up/Home; não é background (M:36). Hoje arte horizontal existe, mas parser usa poster/background/logo (`src/descoberta.c:1173-1233`); menção em comentário Home não prova ingestão (`src/home.c:6776-6786`). **Esboço:** campo/referência de arte independente e política de card existente, preservar preferência manual/origem e cache. **M/médio**, C+W; deps orçamento de textura/URLs; servidor nenhum novo; relacionado **#433** parcial, teste de formato ainda obrigatório.
- **`posterShape` de coleção e arte de catálogo `poster/background`:** tile adequado e capa da biblioteca (C:83-85; M:100-108). Hoje shape nas coleções da conta (`src/colecoes.c:606-704`), não entrada Stremio; Decl enumera nome/tipo/id sem essa arte (`src/descoberta.c:343-359`). **Esboço:** guardar em declaração/fileira/adapter coleção, sem forçar poster horizontal para grade vertical. **M/médio**, C+W; deps M1/C1; servidor nenhum novo; **#433/#468** relacionados, nenhum fechamento garantido.

### M2 — Texto, país e classificação

**O que/ganho:** `originalTitle` mostra nome original; `tagline` frase na ficha; `countries` lista de países; `certification` e `certificationLocal` classificação geral/local. Respeitam o metadata escolhido em vez de novas consultas (M:37-46).
**Hoje/gap:** `src/descoberta.c:1235-1323` lê campos tradicionais como `country`, cast/director; ficha/classificação também vem de TMDB/Jellyfin (`src/jellyfin.c:374-430`). Não consome esses cinco campos Stremio.
**Esboço:** preencher ficha leve pelo addon de origem e fallback existente só se ausente; classificação local não vira filtro parental seguro quando falta/é inválida. Campos de país são strings/listas, não geoblocking. **SUPOSIÇÃO:** título original/tagline só em detalhe para não inflar todos os cards.
**Porte/risco/deps/issues:** **S–M/médio**, C+W; deps presença/validação e regra parental; servidor nenhum novo. Sem issue exata; **#250** é menu árabe, não se fecha por metadata em árabe já suportado.

### M3 — Pessoas e créditos

**O que/ganho:** `people[].name/role/character/photo`; roles actor/director/writer/producer/composer/creator/guestStar, outros ignorados. Cast/crew com foto e personagem sem round-trips extras (M:54-62).
**Hoje/gap:** elenco até 12 com papel/foto e direção (`src/catalogo.h:34-71`); entrada cast/director e enriquecimento, não os sete roles desse objeto.
**Esboço:** parser comum catálogo/ficha; actors/guestStar na fileira atual, direção na seção existente; **SUPOSIÇÃO:** demais créditos em detalhe sob demanda, não fingir compositor como ator. Validar nome/role, imagens acessíveis sem supor headers de auth.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps M2/orçamento da ficha, servidor nenhum novo. Sem issue específica; não refazer filmografia/recomendação existente.

### M4 — Notas públicas, tags, studios/networks

**O que/ganho:** `criticRating` 0–100, `studios/networks` listas (networks depois de studios), `tags` listas. Ganho é contextualizar score/créditos e conservar o que addon apresenta (M:42-46).
**Hoje/gap:** notas multissource/MDBList já existem (`src/extras.c:645-719`, `src/notasfontes.c:17-29`); networks/produtoras TMDB já em extras (`src/extras.c:224-227`), mas esses campos não são ingeridos pelo parser Stremio.
**Esboço:** criticRating como score de crítico com procedência addon, **não rotular Rotten Tomatoes sem fonte definida** e não sobrescrever IMDb 0–10; tags separadas de genres e tags de acesso AIOMetadata. Reusar seções/visibilidade existentes. **SUPOSIÇÃO:** campos ricos ficam na ficha, listas trazem só o necessário.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps M2/procedência, nenhum servidor novo. **#406** candidato parcial a fechar após confirmar pedido; **#434** ocultar gêneros é preferência separada; **#348** título ambíguo, perguntar antes de associar.

### M5 — Estado temporal e temporadas ricas

**O que/ganho:** `endDate` fim da série; `airDays` Sunday…Saturday e `airTime` horário; `seasons[].season/name/overview/poster/released`, season 0 specials e nome padrão se ausente. Melhor contexto, temporadas identificáveis e estreias (M:47-50,79-84).
**Hoje/gap:** números de temporada/datas por vídeos existem (`src/catalogo.h:100-110`,159-190); parser Stremio exclui T0 e não guarda metadata de season (`src/descoberta.c:6444-6451`,6581-6652). Agenda usa dia Cinemeta, não hora (`src/agenda.c:1244-1255`).
**Esboço:** manter rótulos/arte em ficha por temporada, abrir specials sem coercer null→1; default quando temporada não listada. Preservar timestamp/date-only separadamente. `airTime` sem fuso apenas informativo; liberação real usa released/airsAt com contrato confirmado. Não tratar series endDate como garantia de cancelamento.
**Porte/risco/deps/issues:** **M–L/alto**, C+W; deps W9/datas/vistos T0; servidor nenhum novo. **#391** diagnóstico/reprodução necessários; **#356** já lançado, regressão; não prometer que só seasons resolve lista de episódios incompleta.

### M6 — Vídeos: rating, IDs, people, filler e recap

**O que/ganho:** `videos[].rating` número/string 0–10, `ids` próprios do episódio, `people` créditos específicos, `filler/recap` flags de episódio. Episódios informativos e autoplay opcionalmente pula fillers/recaps (M:86-98).
**Hoje/gap:** nome, vid, runtime, data, thumb e nota já existem; nota atual vem do TMDB (`src/catalogo.h:159-190`, `src/descoberta.c:6581-6652`). Não ingere rating/ids/people/flags do addon.
**Esboço:** ler nota com número/string, zero válido separado de desconhecido no modelo novo; preservar IDs/créditos por vídeo, sem enviá-los como IDs da série em watch_state. Flags no snapshot de episódios e decisão de próximo, mas **recap episódio ≠ marcador de trecho recap**. **SUPOSIÇÃO:** mostrar badges primeiro; auto-skip de episódio só com opção do dono, respeitando navegação/contagem de vistos e specials.
**Porte/risco/deps/issues:** nota/flags **S–M/médio**, IDs/créditos **M/alto**, C+W; deps M3/W9; servidor nenhum novo. **#406** parcial; **#329/#430** já lançados tratam trechos, não fechamento de filler novo.

### M7 — Trailers e trailerStreams

**O que/ganho:** `trailers[].source/type/name`; source YouTube ou URL, type Trailer para YouTube, outro tipo para URL. Nome fallback Trailer; complementar por `trailerStreams`, dedup e vídeos diretos primeiro (M:64-77).
**Hoje/gap:** trailer TMDB/IMDb/YouTube e seção já existem (`src/extras.h:191-205`, `src/extras.c:1006-1013`,1120-1137; `src/detail.c:2072-2079`), não leitura desse meta addon.
**Esboço:** adaptar lista para candidatos existentes, URLs diretas no player de trailer, preservar nomes/ordem e isolamento de sessão, não abrir vídeo como título assistido. Reusar fallback/provider configurado; não aumentar quantidade sem limite de memória. YouTube nativo depende dos caminhos atuais, não de um codec novo.
**Porte/risco/deps/issues:** **M/médio-alto**, C+W com limites de trailer por plataforma; deps snapshot/anti-trailer; servidor só proxies de trailer existentes quando usados. **#353** regressão possível, causa não atribuída; #417 já lançado.

## 6. Coleções do addon, não uma segunda Biblioteca

### C1 — Identificar coleção e arte própria

**O que/ganho:** qualquer meta com `collection` é coleção, mesmo type movie; `collection:{}` no catálogo permite abrir como pasta sem tentar tocar. Catálogo só de coleções vira Collections; misto continua biblioteca movie. Arte/nome/descrição/logo/shape são da coleção (C:28-48,72-85).
**Hoje/gap:** `deMeta` não guarda marca (`src/descoberta.c:1167-1323`); conta tem pastas/vitrine (`src/colecoes.c:606-704`, `src/vertudo.c:339-438`) e saga TMDB, não bloco AIO.
**Esboço:** flag/referência de bloco e adapter para navegação existente; catálogo com bloco vazio carrega meta ao abrir. Não sincronizar coleção recebida como coleção editável da conta; não fabricar resource `collection` nem type obrigatório `collection`.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps M1 e meta por origem; nenhum servidor novo. **#362** vitrine já lançada é reuso/regressão; sem issue aberta específica de bloco.

### C2 — `items` e identidade dos membros

**O que/ganho:** `collection.items` são entradas de catálogo, id/type obrigatórios; filmes, séries e outras coleções continuam o mesmo título/histórico em qualquer lugar (C:50-60).
**Hoje/gap:** ColecaoFonte atual navega sources, não lista embutida/nested (`src/colecoes.c:606-704`, `src/vertudo.c:353-438`).
**Esboço:** publicar items pela grade atual, abrir série em temporadas, coleção em drill-down; manter origem para resolver meta. **SUPOSIÇÃO:** guard de ciclos por identidade addon+type+id e profundidade finita, sem copiar árvore inteira para RAM. Itens incompletos enriquecidos ao abrir, sem fetch em fan-out de todo catálogo.
**Porte/risco/deps/issues:** items **M/médio**, nested **L/médio-alto**, C+W; deps C1/W9; servidor nenhum novo. Sem issue exclusiva; preserva #362.

### C3 — `sources`, gênero, addon instalado e paginação

**O que/ganho:** sources com `type/catalogId`, opcional `genre` se extra declarado; `addonId` manifesto ou URL, ausente próprio addon, não instalado skip. Catálogo oculto pode ser source; sources paginadas em ordem (C:55-70).
**Hoje/gap:** resolve addon e abas/fileiras (`src/vertudo.c:353-438`), pagina `genre/skip`, raw-count e dedup ID+type (`src/descoberta.c:8026-8065`), mas não do bloco meta.
**Esboço:** adapter para leitor existente, localizar somente addon instalado, respeitar catálogo/gênero manifestado e manter config query da URL. Não auto-instalar URL em collection. `JELLYFIN_MAX_CATALOG_ITEMS` é limite do servidor AIO, não campo de resposta nem tamanho que devemos alocar; manter janela local e aviso de teto. **SUPOSIÇÃO:** streaming de páginas sob demanda é suficiente para primeiro sources-only.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps C1/declarações; servidor nenhum novo. **#280/#365** são bugs de fileira/catálogo, não fecham com adapter; preservá-los em regressão.

### C4 — União ordenada, dedup e reescrita pelo agregador

**O que/ganho:** items primeiro, sources em sequência, duplicado fica na primeira ocorrência. AIOStreams reescreve catalogId para namespace servido e remove addonId; não recuperar ID original por heurística (C:50-53,87-88).
**Hoje/gap:** Ver Tudo abre sources separadas; dedup numa página/fonte existe, união de todas as sources não é contrato atual (`src/descoberta.c:8026-8065`, `src/vertudo.c:353-438`).
**Esboço:** cursor **interno** por source/skip, identidade type+id+origem quando necessária para não colidir addons, membro já encontrado não duplica; usuário pode continuar carregando sem reordenar. O cursor não é campo wire Stremio. **SUPOSIÇÃO:** dedup do título por ID/type no mesmo namespace; aliases entre namespaces só quando ids confirmam.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps C2/C3/W9, servidor nenhum novo. Sem issue exclusiva; não confundir dedup de títulos com fontes **#458**.

### C5 — Legado sem bloco

**O que/ganho:** meta movie com vídeos-filmes e catálogo com “collection” no nome/id/type ainda pode ser coleção; só filmes, outros membros exigem bloco (C:90-94).
**Hoje/gap:** heurísticas de vídeos/coleções da conta existem, mas não essa regra explícita Stremio (`src/descoberta.c:6581-6652`, `src/colecoes.c:606-704`).
**Esboço:** fallback restrito apenas após não haver bloco; não transformar qualquer movie com vídeos em franquia. **SUPOSIÇÃO:** adiar heurística até ter fixture real que exige compatibilidade, preferir C1 explícito.
**Porte/risco/deps/issues:** **S/médio**, C+W; deps C1 e fixture; servidor nenhum novo; sem issue exclusiva.

## 7. Streams — confiança em dado explícito, não no formatter

### S1 — Service, cached e seeders

**O que/ganho:** `behaviorHints.service` serviço, `cached` disponibilidade nesse serviço, `seeders` número. Valores explícitos reduzem erros de autoplay/ordem por texto (S:28-40,53-57).
**Hoje/gap:** inferência textual de cache/semeadores; `clientResolve.isCached:false` parcial; campos documentados ignorados (`src/stream_parse.c:265-284`,373-395).
**Esboço:** ler hints ancorados e distinguir ausente/false/zero. Estruturado válido vence inferência, mantém fallback textual quando ausente; cached:false não vira fonte pronta. Services documentados: realdebrid, alldebrid, premiumize, debridlink, torbox, easydebrid, debrider, offcloud, pikpak, putio, seedr, torrin, easynews, nzbdav, altmount, stremio_nntp, stremthru_newz, aiostreams. Guardar/display não implica implementar resolvedor para cada um: URL já resolvida toca sem conta local; hash puro depende de debrid/P2P existentes (`src/streams.c:471-480`, `src/debrid.c:86-93`).
**Porte/risco/deps/issues:** **S–M/médio**, C+W, servidor nenhum novo; depende parser raiz/precedência. **#402** candidata parcial só com fixture do relato; não refazer **#355** texto completo já lançado.

### S2 — Indexer, publishDate, duration e bitrate por arquivo

**O que/ganho:** `indexer` origem, `publishDate` ISO 8601 idade da release, `duration` segundos runtime/bitrate com videoSize. Ajuda escolher corte/qualidade com números confiáveis (S:35-40,61-82).
**Hoje/gap:** nenhum consumidor estruturado desses hints (`src/stream_parse.c:344-396`); StreamFit usa duração do alvo, comum às fontes (`src/streams.c:145-223`).
**Esboço:** tamanho bytes exato/duração seconds → Mbps; validar finitude/positivo/unidades e fuso, preservar procedência arquivo versus meta e usar duração real do pipeline ao tocar. Nunca interpretar 1228 s como 1228 min nem transformar taxa média em pico garantido. Indexer/data na folha de fonte; **SUPOSIÇÃO:** filtros de idade só se dono pedir, não adicionar knobs especulativos.
**Porte/risco/deps/issues:** parsing **S**, runtime por versão **M/médio**, C+W; depende correção datas/fit, nenhum servidor novo. **#457** resumo fonte pré-resolvida, parcial; apoia W2 e markers A4.

### S3 — FolderName, folderSize e hints tradicionais

**O que/ganho:** `folderName` complementa filename e filtros keyword/regex; `folderSize` bytes diferencia pacote de temporada. `filename/videoSize` explícitos têm prioridade sobre descrição (S:38-45).
**Hoje/gap:** filename e bytes já lidos, mas filename não ancorado; folder ignorado. Regex monta provedor/rótulo/descrição/arquivo/url/hash, sem pasta (`src/stream_parse.c:326-349`,383-395; `src/streams.c:1051-1055`).
**Esboço:** ancorar filename/videoSize em behaviorHints, pasta separada para detecção/filtro; nunca substituir tamanho de arquivo por folderSize, nem induzir download do pack. **SUPOSIÇÃO:** conservar nomes para detalhes de fonte e matching, sem nova árvore de torrent.
**Porte/risco/deps/issues:** **S–M/médio**, C+W; deps S1/S2, servidor nenhum novo. **#457** parcial, #310 filtros existentes em regressão; não implementa todos os pedidos adicionais #310.

### S4 — URL com infoHash/fileIdx e dedup transversal

**O que/ganho:** torrent URL pode carregar também `infoHash/fileIdx`; Stremio toca URL, AIO identifica mesmo arquivo de outros addons (S:47-48). Evita tentativas repetidas e aba All duplicada.
**Hoje/gap:** parser só extrai hash/índice no ramo sem HTTP (`src/stream_parse.c:298-312`); `mesmaFonte` inclui provedor e rótulo, é retenção de foco, não dedup global (`src/streams.c:2883-2892`).
**Esboço:** preservar URL+identidade em paralelo; **SUPOSIÇÃO:** dedup de delivery seguro por URL exata+headers equivalentes ou hash válido+índice conhecido para agrupar o mesmo arquivo, sem confundir equivalência do arquivo com disponibilidade/credenciais do provedor. Para hash mesmo/URL diferente, manter alternativas recuperáveis se um provider falhar; prioridade escolhe representante, não apaga delivery válida. Índice ausente não equivale a 0; arquivo distinto no mesmo torrent não une. Preservar foco/ordem original, texto e fonte morta por delivery.
**Porte/risco/deps/issues:** parser **S**, dedup UI/autoplay **M/alto**, C+W; deps S1 e política aprovada, servidor nenhum novo. **#458** pode fechar seu recorte de duplicatas após testar fallback e prioridade; agrupamento por qualidade existente não é novo redesign de All.

### S5 — Binge, texto, codec/HDR/áudio e fonte pré-resolvida

**O que/ganho:** hints tradicionais já presentes (`bingeGroup/filename/videoSize`) e media info por versão permitem repetir release entre episódios e mostrar resumo sem nova consulta. J define `MediaSources[].aiostreams.bingeGroup`; A descreve codec/canais/range/idiomas/size/bitrate/runtime (J:89-113; A:89-93).
**Hoje/gap:** binge ancorado e memória existem (`src/stream_parse.c:327-349`, `src/fontepref.c`); selos por texto (`src/badges.c:31-67`); escolha HDR por aparelho/perfil inferido (`src/streams.c:786-850`). Ficha ainda não mostra todos os dados da fonte antecipada (#457, `docs/issues/mapa.json:9133`). Não há schema bruto AIO video/audio/formats/dvprofile nas cópias.
**Esboço:** reusar Stream/selos/cache da fonte antecipada na ficha, claramente “fonte prevista”, sem sonda duplicada. Diferenciar declarado pelo addon, container confirmado e pipeline real; metadata nunca garante DV/Atmos. Ler J versions por contrato explícito (J2); demais dados novos só depois de fixture/schema. Não trocar globalmente User-Agent para fingir AIOStreams.
**Porte/risco/deps/issues:** resumo atual **M/médio**, dados novos **M–L/alto**, C+W, J quando negociado; deps S2/J2/prova de schema, nenhum servidor novo. **#457** candidato a fechar; **#349** preferência MP4/P2P continua ação separada; **#397** codecs/passthrough não se resolve com labels.

### S6 — Headers de mídia e origem segura

**O que/ganho:** o código já recebe `behaviorHints.proxyHeaders.request`; essencial para fontes que precisam Cookie/Referer/Authorization. As fontes não definem schema completo de proxyHeaders/response: expansão é **SUPOSIÇÃO de contrato**, não extensão nova inventada.
**Hoje/gap:** parser 512 bytes e parsing manual (`src/stream_parse.c:150-212`); sonda repassa até sete headers (`src/streams.c:1218-1248`). Android propriedades HTTP (`android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt:395-418`); webOS só Referer/UA/Cookie (`src/video.c:2342-2401`); .tpk só UA/Cookie (`tizen-tpk/Video.cs:409-418`); .wgt no-op (`src/video_tizen.c:1560-1563`). MKV/ASS/legenda externa podem baixar sem headers (`src/mkv.c:727-735`, `src/mkvass.c:658-659`, `src/legenda.c:655`). `proxyHeaders.response` não é aplicado.
**Esboço:** primeiro parsing bounded/escapes/controles sem truncamento silencioso e snapshot para leituras laterais. Reusar Range com stripping cross-origin (`src/rede.h:116-128`); nunca repassar token Jellyfin ao CDN por conveniência. Avisar fonte incompatível com backend em vez de prometer header arbitrário. Overrides response/proxy de VOD só com contrato e necessidade comprovada; o Worker público não reproxy vídeo.
**Porte/risco/deps/issues:** correções/leituras laterais **M/alto**, paridade TPK/WGT/proxy **L/alto**, PC para teste e TVs para prova; deps schema/CDN/APIs, servidor nenhum novo no recorte mínimo. Sem issue exata; preserva fonte/ASS e **#457/#458** não devem retirar headers.

## 8. Subtitles — metadados externos e anexos por versão

### U1 — `lang` e `label`

**O que/ganho:** lang ISO 639-2 ou nome; label título/release; sem label, complemento de lang vira título (“Chinese 2”). Usuário distingue arquivos e escolhe sincronização certa (U:28-43).
**Hoje/gap:** idioma normalizado, arquivo de `subtitleFileName/movieReleaseName`, rótulo truncado; label/complemento não preservados (`src/addons.c:1151-1181`, `src/addons.h:155-167`).
**Esboço:** um leitor de entrada de legenda, label explícito primeiro, aliases antigos depois, idioma/título separados; manter texto integral bounded para detalhe e matching, não cortar antes de escolher release. Reusar `linguas`, `legauto` e UI atuais.
**Porte/risco/deps/issues:** **S–M/médio**, C+W; deps U3/seleção, nenhum servidor novo. **#355** era texto de fonte já lançado; #383 memória de legenda é regressão, não nova entrega do pedido.

### U2 — `forced` e `hearingImpaired`

**O que/ganho:** flags explícitas; sem flags, título Forced/SDH/[HI] infere. Forced cobre linhas estrangeiras; SDH/HI serve acessibilidade (U:35-43).
**Hoje/gap:** externas descartam flags; embutidas já têm MKV Forced/HI/BCP47 e Android roles (`src/mkv.c:98-105`,157-182; `src/faixasmkv.c:100-153`, `src/video_android.c:323-360`).
**Esboço:** tri-state ausente/false/true para não contrariar false explícito; inferência só na ausência, aplicar classe externa na mesma política de seleção/badges. Não confundir Signs com Forced automaticamente nem remover SDH que a pessoa prefere.
**Porte/risco/deps/issues:** **M/médio**, C+W; deps U1/política de idioma/memória, nenhum servidor novo. Sem issue exata; **#429/#425/#383** já lançados entram em regressão.

### U3 — Legendas em `stream.subtitles`

**O que/ganho:** mesmos campos do resource em cada stream, usualmente já casados ao release. Menos busca e mais precisão (S:50-51; U:28-30).
**Hoje/gap:** Stream não guarda anexos (`src/streams.h:20-99`), apenas recurso; leitor recursivo de url pode pegar URL de legenda antes da principal (`src/stream_parse.c:298-326`, `src/js.h:74-100`).
**Esboço:** ancorar campos de raiz antes de ler anexos; parser único U1/U2, ownership por alvo+fonte+geração. Reusar merge/dedup do recurso sem substituir legenda ativa; anexos da fonte A não sobrevivem como disponíveis da B. Definir cópia/liberação junto com Stream/cache, não ponteiro pendurado em struct POD copiada.
**Porte/risco/deps/issues:** **M/alto**, C+W; deps U1/U2 e cópias/cache, nenhum servidor novo. Sem issue exclusiva; pode melhorar matching, não fecha **#374** sync por linha.

### U4 — Matching por hash/size/name e anime

**O que/ganho:** AIOMetadata usa anexos+addons de legendas, hash/size/name e também IMDb para anime Kitsu/MAL; distribuição round-robin evita uma língua consumir tudo (A:95-102).
**Hoje/gap:** extras e dois Ranges de 64 KiB já existem, merge não desloca índices (`src/addons.c:1222-1304`,1805-1845; `src/app.c:2994-3020`). Não mede hash com headers e não faz lookup IMDb adicional de anime nesse caminho; teto cheio pode barrar resultados melhores.
**Esboço:** reutilizar extras; usar IMDb já conhecido M1 para consulta secundária sem nova API de mapping; pool limitado e prioridade do release preservando seleção ativa. Hash de arquivo `videoHash` **não é infoHash torrent**. Header/Range seguros S6; não calcular hash se resposta incompleta ou tamanho falso.
**Porte/risco/deps/issues:** **M/médio-alto**, C+W; deps W9/S6/U3, nenhum servidor novo. **#467** tracking não se fecha por legendas; #383/#429 regressões, sem issue exclusiva.

### U5 — Formatos/conversão/cache

**O que/ganho:** AIO converte SRT/VTT/ASS; AIOMetadata também JSON, cache de corpo e limites por idioma. AIO preserva ASS no caminho servidor só quando URL termina .ass (U:45-47; A:102,264-272).
**Hoje/gap:** nosso cliente detecta SRT/VTT/ASS por **conteúdo**, pode preservar ASS intacto mesmo sem extensão; JSON não tem decoder (`src/legenda.c:78-126`,490-496,651-692`). libass depende do build (`src/assrender.c:114-116`). Não somos servidor de conversão.
**Esboço:** preferir formato textual já suportado na entrega Jellyfin, testar conteúdo/charset sem extensão; JSON só se houver schema/fixture, não presumir array de cues. Reusar caches/overlay bounded; não importar 64 MB de cache externo como configuração nativa padrão. Faixas de imagem seguem suporte do backend; não fingir seleção por índice do addon.
**Porte/risco/deps/issues:** textual **S/médio**, JSON novo **M/alto** dependente de contrato; C+W/J. Servidor externo converte quando disponível, nenhum novo. **#394/#398/#462** são render/performance/AutoSync, não solucionados por flags ou formato anunciado.

## 9. Jellyfin AIOStreams — descobrir features antes de usar

### J1 — Discovery e login/apresentação

**O que/ganho:** `/System/Info/Public.aiostreams` traz `logo/configureUrl/pinSignIn/personal/version{tag,channel,commit}/features`; missing=off, feature valor é versão. Logo/configuração/About e opções só onde servidor permite (J:35-87).
**Hoje/gap:** `jf_publico` lê só Id/ServerName/Version/ProductName (`src/jellyfin.c:212-238`). Login padrão/senha/Quick Connect existem, não os fluxos AIO especiais.
**Esboço:** capability snapshot por conexão e versão conhecida da feature, sem detectar por nome AIOStreams. Logo/configureUrl opcionais e tratados como URL externa, sem executá-los automaticamente; versão diagnóstica sem segredo. **SUPOSIÇÃO:** UI de configurações usa caminho externo já disponível, não navegador web embutido novo. `/AIOStreams/*` usa token de usuário normal, não API key de ferramenta recusada pelo servidor.
**Porte/risco/deps/issues:** discovery **S**, UI/login **M/médio**, J/N; deps conexão e fixture Public. Servidor anuncia, nenhum nosso. Base para **#468** e demais J; não resolve login PIN sozinho.

### J2 — Versions, refresh e resolução lazy

**O que/ganho:** MediaSources[].aiostreams `id/name/description/bingeGroup`, versão estável para retomar; `Fresh/Refresh/MediaSourceId` em PlaybackInfo; resolver só ao play/pedido MediaSources (J:89-113).
**Hoje/gap:** até oito saídas DirectPlay/transcode, nome/codecs básicos, grupo método genérico; sem fields AIO/Fresh/Refresh (`src/jellyfin.c:723-819`). URL é sintetizada `/Videos/.../stream`, sessão encontrada por URL exata (`src/jellyfin.c:1742-1758`).
**Esboço:** identificar versão separada de item/fonte técnica/URL e preservar sessão após redirect; usar name/description/bingeGroup na folha/memória atuais. Fresh ao listar e Refresh solicitado, não consultar no detalhe inteiro. Revalidar seleção via MediaSourceId antes de tocar. **SUPOSIÇÃO:** URL direta de servidor virtual e auth exigem fixture de PlaybackInfo+redirect; não aceitar filesystem Path nem anexar api_key Jellyfin a URL de CDN. Mais de oito versões pede paginação/janela de saídas, não só constante maior.
**Porte/risco/deps/issues:** **M–L/alto**, J/N; deps J1, identidade/sessão, S5/S6/U3. Servidor deve oferecer versões; AIOMetadata não transcoda. **#457** parcial e #310 binge já existente em regressão; sem issue exclusiva.

### J3 — Bibliotecas, BoxSet, genreRequired e busca

**O que/ganho:** bibliotecas Collections/BoxSet, anime/pastas não tipadas; `CollectionFolder.aiostreams.genreRequired`, `/Genres?ParentId`, None=all apenas quando oferecido. Mostrar coleções/catálogos realmente configurados, inclusive dependentes de gênero (J:115-128; A:64-83).
**Hoje/gap:** só IDs hex e CollectionType movies/tvshows (`src/jellyfin.c:434-457`), Items só Movie/Series (`src/jellyfin.c:490-505`); worker limitado a seis bibliotecas/24 itens e pula vazias (`src/jellyfin.c:1540-1572`). Busca Jellyfin hoje local à Home (`src/busca.c:575-598`).
**Esboço:** primeiro fixture de #468 e diagnóstico de biblioteca filtrada/teto/gênero/vazio, sem URL/token. Depois modelar folder/BoxSet/type sem forçar todos a filme; browse por parent e janela existente, gênero selecionado antes do fetch, sem tratar vazio condicionado como inexistência. Buscar no servidor por contrato padrão **a confirmar em fixture**, não achar que as 24 entradas são a biblioteca completa. IDs virtuais não hex exigem contrato antes de relaxar validador (`src/jfid.h:41-64`).
**Porte/risco/deps/issues:** diagnóstico **S**, suporte completo **L/alto**, J/N; deps J1/C1/navigation e fixtures IDs, nenhum servidor nosso. **#468** candidata a fechar só após reproduzir as bibliotecas ausentes; **#405** busca rápida geral relacionada, não causa provada; **#280/#365** regressões.

### J4 — Episódios filler/recap

**O que/ganho:** com feature fillers, Episode.aiostreams `filler/recap`; badges e next automático pode pulá-los com opção de desligar (J:123-128).
**Hoje/gap:** `jf_episodios` lê T/E/data/runtime/thumb, não essas flags (`src/jellyfin.c:560-608`). Trechos de recap já tratados pelo intro, não episódios recap.
**Esboço:** alimentar mesmo snapshot M6/política de next; não converter episódio pulado em assistido, respeitar specials/unaired e loop prevention.
**Porte/risco/deps/issues:** **M/médio**, J/N; deps J1/M6/W9; nenhum servidor novo. Sem issue exclusiva; #329/#331/#430 regressões, já lançados.

### J5 — Usuários e troca autenticada

**O que/ganho:** GET `/AIOStreams/Users` com `user/avatar/hidden/needs` null/pin/password/password-pin; POST `/AIOStreams/Token {UserId,Pw}` responde auth padrão, 401/404/429. Família troca identidade sem trocar catálogo indevidamente (J:130-150).
**Hoje/gap:** um servidor por perfil Nuvio e login padrão (`src/jellyfin.h:12-34`, `src/jellyfin.c:331-350`), não picker AIO.
**Esboço:** negociar users, selector integrado ao vínculo de perfil, token por usuário/dispositivo; pedir segredo correto, apagar após request, 429 respeitado. Não carregar hidden users fora da resposta autorizada nem herdar token do owner. Troca cancela jobs e snapshots anteriores.
**Porte/risco/deps/issues:** **M–L/alto**, J/N; deps J1/W8 e UI aprovada; servidor AIO/AIOMetadata com feature, nenhum nosso. **#466** não depende de construir esse picker; benefício familiar sem issue específica.

### J6 — PlayedUpTo

**O que/ganho:** POST `/AIOStreams/PlayedUpTo/{episodeId}`→204 marca episódio e anteriores da série, exclui especiais e não lançados; sem feature não oferecer ação (J:217-221).
**Hoje/gap:** marcas de lote locais em `src/episodios.c:312-356`, sem endpoint AIO.
**Esboço:** ação existente adaptada para item Jellyfin; request único feature-gated, usar ID privado no servidor certo, refletir após aceitação/refresh. Não enviar jf: para addon nem fabricar lotes infinitos como fallback de UI.
**Porte/risco/deps/issues:** **S–M/médio**, J/N; deps J1/J3 e confirmação destrutiva; nenhum servidor novo. Sem issue exclusiva, W10 é contrato separado.

### J7 — Dropped

**O que/ganho:** com feature dropped, POST padrão `/UserItems/{id}/Rating` com Likes=false drop série; Likes=true undrop sem limpar avaliação numérica (J:223-229).
**Hoje/gap:** não há tradução desse estado no cliente; UserData não importa Played/IsFavorite completos (`src/jellyfin.c:423-428`).
**Esboço:** mesma intenção de W11, apenas para série/feature; ocultação local não vira dislike; token servidor, não header addon. Atualizar CW/Next Up sem apagar histórico.
**Porte/risco/deps/issues:** **M/médio**, J/N; deps J1/W6/W11 e produto; nenhum servidor novo. Sem issue exclusiva.

### J8 — Activity, histórico, clear/export

**O que/ganho:** `/AIOStreams/Activity` usuários/counts (`played/movies/episodes/inProgress/favorites/lastAt`), avatar/hidden/locked/historyOf e sessions autorizadas; `/AIOStreams/History?userId=&limit=&cursor=&source=` retorna items/cursor com origem local/import/tracker, playCount/played/positionMs/durationMs/favorite/lastPlayedAt/sortAt/item. Clear `{userId,itemKeys?}`→cleared; Export JSON (J:152-215).
**Hoje/gap:** UI/diário social e histórico local não consomem essas rotas; integração J limitada em `src/jellyfin.h:12-18`, `src/jellyfin.c:374-430`. Social não tem autorização para histórico do servidor.
**Esboço:** só se dono quiser tela de atividade; paginação por cursor documentado, distinguir ms destes dates de at seconds W. Proveniência visível; counts null para histórico compartilhado, sessions só se resposta permite. Clear exige confirmação específica e nunca se propaga a outros trackers implicitamente; export privado para armazenamento escolhido, sem upload social. Sem history, usar fallback de consultas padrão **com schema a confirmar**, não inventar endpoint.
**Porte/risco/deps/issues:** leitura **M**, clear/export/UI completa **L/alto**, J/N; deps J1/J5, navegação e política de privacidade. Servidor existente, nenhum nosso. **#438/#467** tracking não requer tela; sem issue exclusiva, futuro.

### J9 — Login específico AIO

**O que/ganho:** UUID/alias, username `<uuid>/<user>`, password/PIN; configSignIn, pinSignIn e personal em endereços `/jellyfin/<uuid>/<encrypted password>`, `/jellyfin/u/<alias>`, `/jellyfin/p/<code>`. Conexão simplificada, não auth universal Jellyfin (J:231-244).
**Hoje/gap:** senha/Quick Connect e prefixo reverse proxy existem (`src/jellyfin.c:241-350`, `src/jellyfin.h:12-27`); não há esses fluxos específicos.
**Esboço:** manter base configurada integral, jamais registrar caminho sensível. Preferir login padrão/Quick Connect que já funciona; adicionar PIN/personal só se requisito do dono e fixture, sem derivar alias/código/encrypted password.
**Porte/risco/deps/issues:** **M/médio-alto**, J/N; deps J1/J5, fornecedor; nenhum servidor novo, sem issue exclusiva; adiar.

## 10. AIOMetadata como serviço virtual — capacidades além do wire addon

### A1 — Deploy/conexão do servidor externo

**O que/ganho:** browsing+tracking Jellyfin, bibliotecas de addons e vídeo direto; sem mídia armazenada/transcode. `JELLYFIN_API_ENABLED`, stream addon, HTTPS estável, banco playstate; client password/Quick Connect (10 min), token inactivity 30 dias (A:3-36).
**Hoje/gap:** `jf_*` existe, mas perfil pede transcode e synthesis URL presumem servidor real (`src/jellyfin.c:612-640`,757-819). WGT sem HTTP estrito não oferece Jellyfin (`src/jellyfin.h:36-38`).
**Esboço:** validar login/browse/play/check-ins contra AIOMetadata virtual com DirectPlay compatível; não esperar ele converter DTS/TrueHD/DV. Duração real hoje descartada na ponte J (`src/jellyfin.c:1751-1754`,1787-1789); body não envia Item.RunTimeTicks (`src/jellyfin.c:828-841`). **SUPOSIÇÃO:** incluir runtime real no shape documentado A:128 e confirmar fixture melhora conclusão correta. Sem servidor nosso e sem mudar política TLS geral para addon público.
**Porte/risco/deps/issues:** prova **S**, gaps J **M–L/alto**, J/N; deps J1/J2/S6 e divergência tokens; **#468** depende de browsing, **#459/#466** pode usar handoff, não fechar automaticamente.

### A2 — Tags, classificação e usuários compartilhados/independentes

**O que/ganho:** tags filtram catálogo/coleção/search; rating da tag restringe acesso; usuário Same person compartilha histórico, outro mantém seu próprio e contas Simkl/MDBList/PublicMetaDB/AniList/MAL. Trackers/watchlist/skip por card, avatar; catálogo restrito por Users (A:38-83).
**Hoje/gap:** perfis Nuvio não são usuários desse servidor (`src/perfis.c:326-345`); só movie/tvshows da conexão entram em J e sem picker AIO.
**Esboço:** consumir a visão autorizada do usuário conectado; não baixar todas as views e filtrar só na UI. Reusar perfil e J5 para trocar token; metadata pública/criticRating não substitui regra parental. Coleções configuradas do servidor são read-only no cliente; erro de edição aponta configuração, não cria API edit inventada.
**Porte/risco/deps/issues:** **M–L/alto**, J/N; deps J3/J5/W8. Configuração/contas vivem no fornecedor, nenhum Worker nosso. Sem issue exata; protege família em **#466**, não promete reproduzir os filtros do dashboard offline.

### A3 — Tracker picker, snapshots, watchlist e sincronização incremental

**O que/ganho:** escrita para destinos habilitados independente do provider escolhido para leitura; Automatic CW união, watched/Next Up um provider (MDBList→Simkl→PublicMetaDB), One tracker ou This server only. Estado incremental/indexado, outbox durável, timestamps protegem rewatch; watchlist por shelves/catálogos e TTL, favoritos=watchlist (A:124-213).
**Hoje/gap:** Trakt/Simkl/CW/Nuvio existem, mas não picker do serviço; Conta/library não é biblioteca Simkl via simples troca de label (#393). Outbox externo não garante entrega do nosso cliente antes de chegar a ele.
**Esboço:** preferir W adaptador ou J views, não duplicar todo index/outbox AIOMetadata no Worker. Expor provider de leitura explicitamente; limites/refresh explicados ao usuário. Importar Played/IsFavorite em J com propriedade de origem, não só progresso (`src/jellyfin.c:423-428`). **SUPOSIÇÃO:** #438/#467 atendidos por conta externa conectada apenas quando operações pedidas realmente funcionam: tabela A:168-173 lista AniList/MAL só watchlist, enquanto A:46 fala writes; confirmar start/stop/progresso anime, não afirmar histórico completo com base nessa tabela.
**Porte/risco/deps/issues:** consumo **M–L/alto**, J/N ou W; deps W4–W9/J3, serviço externo configurado. **#393/#438/#467/#459** candidatos parciais/alternativos; contratos diretos continuam sem prova offline.

### A4 — Media segments: intro/recap/outro

**O que/ganho:** segmentos prontos do servidor, Automatic PublicMetaDB→AniSkip→IntroDB; duração do arquivo escolhe release AniSkip mais próximo, >10% de diferença recusa; override por usuário (A:108-120).
**Hoje/gap:** IntroDB/AniSkip/chapters já implementados com tolerância 10% e prioridade do próprio arquivo (`src/intro.c:84-129`,426-551, `src/credfonte.c:6-27`). Não importa media segments Jellyfin nem PublicMetaDB nesse leitor.
**Esboço:** alimentar `IntroTrecho` por sessão/fonte e validações existentes, **após obter shape/endpoints Jellyfin em fixture**, não criar `stream.intro` sem spec. Preservar capitulo deste corte e provar ordem de fontes; evitar consulta duplicada quando segmento válido já veio. Botão/auto-skip existentes; esconder botão #337 é preferência de UI distinta.
**Porte/risco/deps/issues:** **M/médio-alto**, J/N; deps S2/identidade/fixture, fornecedor ou fontes existentes; nenhum servidor nosso. #329/#430 já lançados são reuso; **#337** não fecha só com segmentos.

### A5 — Aparência, shaping, CSS e playlists

**O que/ganho:** imagens já configuradas, Bring Posters to 2:3/crop/blur do image cache; custom CSS para clientes web, temas; playlists não servidas (A:228-232,302-312).
**Hoje/gap:** app SDL/GLES não é web app Jellyfin; já tem arte/fallback/escolha (`src/artefontes.h:8-66`, `src/arteescolha.h:10-25`) e shapes locais de coleção.
**Esboço:** consumir imagens do servidor e M1 sem crop duplicado obrigatório; **não implementar CSS remoto na UI C** nem playlist que o fornecedor não serve. Para fonte/image autenticada, schema/credencial por URL precisa fixture; o cache atual baixa sem headers (`src/tex_cache.c:1625-1646`).
**Porte/risco/deps/issues:** arte **S–M/médio**, CSS/playlists **não implementar**, C+W/J; deps M1, fornecedor image cache; nenhum servidor novo. **#433** formato/arte parcial ainda precisa prova; nenhum fechamento por CSS.

### A6 — Operação, quotas e diagnóstico de instância

**O que/ganho:** limites/TTL evitam fan-out e tornam histórico/subtitles frescos; retry serial respeita Simkl per-user lock/um write por segundo, concorrência entre contas; catálogo watchlist pode ter cache de um dia. Não confundir playstate sync com TTL watchlist (A:144,185-213,238-300,314-340).
**Hoje/gap:** cliente tem quotas próprias e logs sanitizados, mas não conhece frescor da cadeia externa (`src/addons.c:1189-1194`, `src/descoberta.h:217`, `src/jellyfin.h:96-111`).
**Esboço:** documentar no teste/deploy externo os controles abaixo, sem criar a mesma matriz de knobs no app. Cliente honra `pull.ttlSeconds`/HTTP Retry-After e não pede leitura completa a cada quadro. UI de reconexão/dados indisponíveis preserva snapshot; instrução operacional não é causa comprovada de #451/#468.
**Porte/risco/deps/issues:** **S/médio**, N/W/J com limitações; deps W3/W4 e logs sem credenciais; servidor externo configura quotas/banco/cache. **#451/#468/#459** relacionados, nenhum fechado só por configuração.

**Inventário operacional do fornecedor (não campos do manifesto nem features novas do cliente):**

- Server: `JELLYFIN_API_ENABLED`, `JELLYFIN_CUSTOM_CSS`, `JELLYFIN_TOKEN_TTL`, `JELLYFIN_QUICK_CONNECT_TTL`, `JELLYFIN_CATALOG_MIN_PAGE`, `JELLYFIN_SHELF_META_CONCURRENCY` (A:238-247).
- Playback: `JELLYFIN_STREAM_USER_AGENT`, `JELLYFIN_RESOLVE_ON_OPEN`, `JELLYFIN_STREAM_TIMEOUT_MS`, `JELLYFIN_STREAM_CACHE_TTL`, `JELLYFIN_MAX_MEDIA_SOURCES`, `JELLYFIN_MAX_TRAILERS`, `TRAILER_ADDON_TIMEOUT_MS/TRAILER_ADDON_TTL/TRAILER_ADDON_EMPTY_TTL`, `JELLYFIN_SEGMENTS_TTL`, `INTRODB_TIMEOUT_MS/ANISKIP_TIMEOUT_MS`, `INTRODB_BASE_URL/ANISKIP_BASE_URL` (A:249-262).
- Subtitles: `JELLYFIN_SUBTITLES_PER_LANGUAGE`, `JELLYFIN_SUBTITLES_MAX`, `JELLYFIN_SUBTITLE_TTL`, `JELLYFIN_SUBTITLE_FETCH_TIMEOUT_MS`, `JELLYFIN_SUBTITLE_BODY_CACHE_MB` (A:264-272).
- History: `JELLYFIN_PLAYED_THRESHOLD`, `JELLYFIN_PROGRESS_WRITE_INTERVAL`, `PLAYBACK_MARK_REPEAT_WINDOW`, `JELLYFIN_PLAYSTATE_SYNC_INTERVAL/DELAY/ACTIVE_HOURS/CONCURRENCY/BUDGET/USER_TIMEOUT`, `JELLYFIN_WATCHED_REFRESH`, `JELLYFIN_RESUME_TTL/MAX_AGE/TIMEOUT_MS`, `JELLYFIN_WATCHED_MAX_PAGES`, `JELLYFIN_NEXTUP_MDBLIST_PAGES/OWN_DAYS`, `JELLYFIN_UPCOMING_DAYS/TTL/LOCAL_SHOWS/WATCHLIST_LIMIT`, `JELLYFIN_WATCHLIST_MEMO_TTL`, `WATCH_STATE_PULL_TTL/ITEMS` (A:274-300).
- Outros: `TRACKER_OUTBOX_CONCURRENCY/MAX_AGE_HOURS/START_WINDOW`, `WATCH_STATE_HANDOFF_ENABLED` (A:144,224); `JELLYFIN_MAX_CATALOG_ITEMS` em C:57; `WATCH_STATE_REPORT_ENABLED`/`WATCH_STATE_PULL_ENABLED` em W:443-444. Não importar coalescing específico AIOMetadata como obrigação universal do W.

## 11. Manifesto/response extras e capacidades sem schema no pacote

**Hoje:** IDs/root strings/URL query preservados, helpers addonurl e metaprov; search em extra/extraSupported, search requerido exclui Home, genre obrigatório ainda tenta sem genre, paginação skip por contagem bruta (`src/addonurl.h:70-244`, `src/descoberta.c:1940-1996`,2268-2308,8026-8065). Busca remota tem 12 resultados/alvo, 16 alvos, três workers, sem paginação (`src/descoberta.c:776-778`,895-915`).

**Implementar agora somente novos campos documentados:** watchState, arte de catálogo, marcador/bloco collection, hints/flags; filtros de watch_state próprios, não assumir que filtros meta cobrem tudo. Não inventar `hasMore`, `next`, delta `removed`, `sessionId`, `sequence`, `updated_at`, `cacheMaxAge/staleRevalidate/staleError`, root `recommendations` ou `configurationRequired/configurable` como contrato destas fontes. **SUPOSIÇÃO:** suporte futuro de configuração remota/headers response/media info depende de nova cópia offline aprovada e fixture; estimativa **M/alto**, nenhum código até isso existir. Nenhuma issue se fecha por campo não documentado.

**Impacto social pequeno, se identidade rica for usada:** backend aceita IMDb `^tt\d+$` e snapshot poster 512, não todos os IDs/URLs 2048 do cliente (`servidor/recomendacoes/src/index.js:690-783`). UI só valida type/nonempty (`src/detail.c:2216-2219`). **SUPOSIÇÃO:** converter por alias IMDb confirmado ou esconder envio indisponível é **S/baixo**, reusa `/v1/rec`; ampliar backend para IDs arbitrários é **M–L/alto** separado, sem necessidade para W/M/C. Nunca enviar IDs privados `jf:/em:/px:` nem URLs-install como identificação social (`src/jfid.h:4-38`).

## 12. Matriz de plataformas e servidor

| Plataforma | O que pode compartilhar | Guarda/lacuna real e ação planejada |
|---|---|---|
| LG webOS nativo | parser, outbox HTTP, snapshots, UI | uMS playing/paused/end existem (`src/video.c:1280-1311`); stop capturado antes de unload. Lifecycle sem espera de POST; testar Home/retido/kill. Headers arbitrários não existem; codecs/HDR pertencem à TV. |
| Samsung .tpk 4/5,6,6.5,8/9 | core e transporte nativo; `Video.cs` compartilhado | OnPause/terminate host (`tizen-tpk/Program.cs:931-952`), evento C só ativo, sem geração por callback (`src/video_tpk.c:273-300`). Ponte mínima lifecycle/sessão, não engine nova em C#. Evitar `_Thread_local` TPK40 (`src/rede.h:226-238`). UA/Cookie apenas. |
| Android Media3 | core/outbox e propriedades HTTP | `NvPlayer` diferencia buffering/ended/intenção (`android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt:1003-1044`). onPause pausa, onDestroy pode matar (`android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt:521-566`); captura não depende do próximo frame SDL nem de onDestroy. JNI callback sem geração geral (`src/video_android.c:539-558`). |
| Samsung .wgt / Tizen web | parsing/UI e storage IDBFS existentes | POST `rede_pedir` estrito indisponível, XHR antigo não honra deadline (`src/rede.h:20-23`, `src/rede.c:212-221`,330-347). **SUPOSIÇÃO:** transporte async no worker/fetch com deadline/status/Retry-After e confirmação de persistência, etapa própria; sem afirmar que Emscripten já dá paridade. |
| VIDAA | web core, mt/SAB ou st, assets/proxy | Experimental sem prova física (`README.md:101-110`). CORS/preflight de JSON POST e mixed-content; fechamento pode parar frames. Proxy GET não resolve push. UI deve dizer indisponível se contrato de transporte não for garantido. |
| PC/Mac/Linux | testes determinísticos de core/HTTP/persistência | vídeo host stub/simulação (`src/video.c:261-334`, `tools/linux.sh:1-5`); `posSeg += dt` fallback não é play externo válido (`src/player.c:3549-3554`). Não prometer cliente de vídeo PC completo nem usar teste host como prova de TV. |

**Persistência:** native temp+rename não garante fsync contra corte de energia; IDBFS tem flush assíncrono/mount falho possível (`src/dados.c:26-37`,113-218,370-386,504-528). Fila no disco ≠ confirmada no IndexedDB. A transição deve ser rápida; IO limitado fora do desenho com snapshot aceito e erro observável. **SUPOSIÇÃO:** checkpoint local periódico pode recuperar posição após kill; ele não cria evento W `progress` e não inventa stop assistido no boot.

**Nosso backend:** `servidor/recomendacoes` é social/identidade, não guarda catálogo global, queue watch_state nem oferece Jellyfin. TV/assets foram separados (`servidor/tv/src/index.js:1-10`,109-132). `servidor/recomendacoes/src/proxy.js:13-24`,35-42 proíbe vídeo/áudio e só GET/OPTIONS, headers/retry limitados. **Primeiro slice não exige D1, migração ou deploy.** Se VIDAA precisar relay POST, será projeto explícito de segurança: autorização, allowlist/origem, quota, tamanho, no-store, Retry-After, proteção SSRF e logs sem segredo; não converter write em GET nem abrir proxy universal. **L/alto, futuro**, só após falha comprovada de CORS direto.

## 13. Primeira fatia executável mínima — proposta 2.0.6

A investigação já terminou; passos abaixo são para uma implementação **futura e autorizada**, não comandos realizados aqui. **SUPOSIÇÃO de cronograma:** promover recorte de #466 (hoje futuro no mapa) somente se couber após bugs do patch, com opção desligada por padrão na introdução e canário nativo primeiro.

### Escopo fechado

- Um destino addon por perfil, v2 explícito, movie/series com identidade exata conhecida; um write path escolhido, sem duplicação com Jellyfin/Trakt direto no mesmo destino upstream conhecido pelo usuário.
- `start/pause/start/stop` e mark individual `played/unplayed`; no bulk, no rated/drop, no nova API tracker direta, **sem pull no primeiro merge**.
- Outbox journal/retry/diagnóstico e fence de pessoa desde início. Só runtime real/confiável; se identidade não conhecida, skip observável em vez de emitir errado.
- Manual watchlist e multi-viewer UI ficam na fase seguinte; perfil secundário herdando URL sem autorização nunca reporta owner.
- Native LG/.tpk/Android; PC como harness. WGT/VIDAA feature indisponível até transport/persistence equivalentes provados.

### Passos e pontos de mudança previstos

- [ ] **1 — Contrato e fixtures puras.** Criar `tests/watchstate.c` e runner `tests/watchstate.sh` no padrão assert+stubs dos testes existentes; relógio/rede/disco fake, sem framework novo. Fixtures manifesto push-only/pull-only/resources objeto, body de W, type/prefix/meta+video, invalid/overflow/null. `src/js.h:74-100`/helpers raiz já existem; só compartilhar helper adicional se dois consumidores reais precisarem.
- [ ] **2 — Adesão/binding.** Modificar `src/addons.c/.h` em `addons_manifesto_lido`, integração da tela `src/addonsui.c` e ajuste local em `src/ajustes.c`; estado explícito por perfil/configuração, não reutilizar enum otimista para write antes da sonda. Não modificar esquema da conta oficial nem `mapa.json`.
- [ ] **3 — Adaptador/outbox pequeno.** Criar `src/watchstate.c/.h` (nome **SUPOSIÇÃO**, não API existente), usar `jsw`, `RedePedido`, `dados` e padrão ACK revision de contapend; journal por usuário/perfil/destino com limites comunicados. Referenciar configuração privada existente e fingerprint no evento interno, não duplicar a URL secreta em cada registro. Carregar somente contexto autorizado; nunca reatribuir evento de perfil 1 ao ativo 2. Capturar body/id/at uma vez; mesmos bytes nos retries.
- [ ] **4 — Eventos e identidade.** Ligar `src/player.c` nas transições provadas e `fecharSessao`, snapshot de metadata original em `src/descoberta.c`/`src/catalogo.c` apenas onde necessário, gestures em `src/visto.c`/`src/episodios.c`/`src/ctxmenu.c`. Não usar tokens Supabase (`src/nuvem.c:93-118`) nem queue Trakt. Filtrar live/trailer/simulação antes da emissão; stop abandonado também existe.
- [ ] **5 — Lifecycle/retenção.** Reusar sinais existentes e acrescentar apenas ponte tipada para core quando frame pode parar (Android/Tizen/main); eventos carregam sessão/geração. **SUPOSIÇÃO recomendada para Home retido:** fim da sessão lógica gera stop uma vez; reabrir retido gera start com novo início, mesmo sem novo load físico. Buffering não é pause. Se dono escolher pause lógico, manter início e testes correspondentes; nunca misturar políticas.
- [ ] **6 — Guard e canário.** Fazer red→green dos testes W, rodar regressões específicas e medir ciclo real em uma instância/TV autorizada. Feature desligável por perfil, sem mudar comportamento Trakt/Nuvio/Simkl existente; após desativar, pausar entrega e manter journal privado até decisão de descarte, sem enviar silenciosamente a outro endpoint.

### IDs/threshold — decisões que precisam ficar visíveis

W só exemplifica gramáticas de `id`; exige igualdade em retry e início/posição para playback, timestamp para marca. **SUPOSIÇÃO:** derivar ID determinístico das classes documentadas e persistir string materializada; teste precisa distinguir rewatch e duplicata de stop. Duas pausas legítimas no mesmo ponto/início podem colidir nessa descrição: confirmar comportamento do fornecedor antes de acrescentar discriminador **interno à string de id**, nunca um campo `sequence` wire inventado.

**SUPOSIÇÃO conservadora até decisão do dono:** `played:true` apenas com threshold escolhido explicitamente e prova anti-falso-visto; posição original permanece. O visto local por créditos/prova é diferente (`src/player.c:2424-2455`). Não mentir 100% para harmonizar destinos; não pegar 90/80 pelo nome do addon. Canário deve documentar divergência e bloquear promoção se fornecedor recalcula e marca incorretamente.

### Critério de parada do primeiro slice

Um filme/episódio legítimo produz a sequência exata; pausa longa não gera spam; stop antes do threshold chega como não concluído; stop concluído é aceito uma vez apesar de retry/restart; marca manual chega; trailer/seek ao fim/perfil errado produzem zero falso visto. Não é necessário pull, AniList OAuth, novas coleções ou novo servidor para declarar esse slice entregue. O status de #466 deve dizer **push entregue; pull/expansões pendentes**, não “integração completa”.

## 14. Roadmap priorizado — sem datas e sem sobrescrever o mapa

| Prioridade/alvo proposto | Entrega independente | Porte/risco | Dependência e fechamento honesto |
|---|---|---|---|
| **P0 / 2.0.6** | Fixture/diagnóstico #468 e estabilizar bugs CW/episódios existentes antes de importar outro provider | S–M/médio | #468 só fecha causa reproduzida; #451/#404/#391 permanecem separados. Não subir tetos às cegas. |
| **P1 / 2.0.6 canário → estável se guard passar** | W1–W3/W8 + identidade convencional, push player/mark individual | M agregado/alto | Primeira fatia §13; move parte de #466 hoje futuro, precisa aprovação do dono. Nenhum servidor novo. Se faltar contrato AIOMetadata v2, validar addon fixture e manter canário, não anunciar interoperabilidade. |
| **P2 / 2.0.6 ou 2.1 conforme capacidade** | S1–S3, preservar URL+hash S4, U1/U2, rating episódio M6 | S–M/médio | Parsers primeiro sem remodelar UI; #402/#406 apenas recortes confirmados. Preparação para #458/#457, não fechamento prematuro. |
| **P3 / 2.1** | W4 pull items → CW, único provider por perfil; depois W5 watched + W7 watchlist por origem | M+L/alto | Não colocar watched destrutivo no patch inicial. Completa núcleo #466; provider picker ajuda #393/#438 após confirmação. |
| **P4 / 2.1** | Dedup delivery/fallback #458, resumo fonte antecipada #457, U3/U4 anexos e release matching | M/alto | Reusa fit/selos/memória/legendas; regressão ordem/foco e headers obrigatória. |
| **P5 / 2.1** | M1–M5 arte/pessoas/metadata de season e C1/C3 sources; C2/C4 items/união em slices seguintes | M–L/médio-alto | Coordenar com Biblioteca/Coleções já previstas, não refazer. #433/#362 são parcialmente/já lançadas, regressão. |
| **P6 / 2.1, integração J maior conforme roadmap do dono** | J1 discovery, J2 versões, J3 bibliotecas/gênero/BoxSet, runtime+legenda por versão | M–L/alto | Fix pontual #468 pode anteceder; expansão servidores locais no roadmap existente é 2.2 (`docs/issues/MAPA.md:93-98`). Não acelerar silenciosamente escopo inteiro para 2.1. |
| **P7 / futuro** | W9 anime/mixed/absolute completo, W10 bulk, W6/W11 drops, W7 ratings completos, v1 confirmado | L agregado/alto | #467 só fecha operações AniList efetivas, não arte/lista; #459 direto exige API confirmada se delegação não basta. |
| **P8 / futuro** | J5/J8/J9 família/history/export/PIN, A4 segments novos, nested/legado C2/C5 | M–L/alto | Owner escolhe produto; dados sensíveis/destrutivos com consentimento. Não construir por ser possível. |
| **P9 / futuro, gate próprio** | WGT/VIDAA transporte/persistência paritária; headers de mídia onde faltam; relay somente se necessário | L/alto | Prova de browser/backend/SSRF/custo, nenhuma promessa de proxy universal. |

**Dependência mínima:** identidade convencional+binding → outbox → transições → push; depois snapshot por origem → items CW → watched/watchlist → bulk/anime/ratings. Metadata/hints simples podem avançar em paralelo; não bloquear push esperando “tudo do mercado”.

## 15. Relação com issues abertas e já lançadas

Consulta somente de títulos/notas/estado do snapshot local, sem ler novos comentários remotos. **Fechar significa após implementar e verificar o recorte**, não como ação deste documento.

| Issue e origem no mapa | Estado/alvo atual | O que este plano atende / não atende |
|---|---|---|
| **#466** `docs/issues/mapa.json:9253` — Incremental watch tracking | aberta/futuro | Push P1, pull P3 e extras P7; fechamento completo exige definir escopo aceito pelo autor/dono. |
| **#467** `docs/issues/mapa.json:9273` — Anilist tracking | aberta/futuro | Delegação W/A3 evita OAuth novo; comprovar anime mapping e operação real. `src/artereserva.c` consultar arte não é tracking. |
| **#459** `docs/issues/mapa.json:9173` — Simkl progresso parcial | aberta/2.1 | W2/A3 pode oferecer modo delegado; usuário pediu Simkl conectado direto. Não inventar endpoint de scrobble Simkl; contrato direto ou aceitação do modo alternativo indispensáveis. |
| **#458** `docs/issues/mapa.json:9153` — Duplicate sources | aberta/2.1 | S4 preserva hash com URL e dedup por prioridade sem apagar fallback; qualidade/All separado do redesign. |
| **#468** `docs/issues/mapa.json:9293` — Jellyfin catalogs | aberta/2.0.6 | J3/A1; provar filtro/teto/gênero/ID/vazio específico. Quatro visíveis não prova teto seis como causa. |
| **#457** `docs/issues/mapa.json:9133` — Fonte pré-resolvida na ficha | aberta/2.1 | S2/S5 e J2, mostra Stream disponível sem baixar fonte outra vez; schema novo ainda não comprovado. |
| **#438** `docs/issues/mapa.json:9026` — MdbList | aberta/futuro | Scores MDBList **já** em extras; tracking/watchlist por AIOMetadata é nova possibilidade. Título vago: confirmar o que falta antes de fechar. |
| **#393** `docs/issues/mapa.json:8134` — Library source Trakt→Simkl | aberta/2.1 | Provider picker/snapshot W/A3; CW/destino de + já têm Simkl. Biblioteca completa exige ampliar fonte certa, não só rótulo. |
| **#406** `docs/issues/mapa.json:8399` — Passing ratings on title | aberta/2.1 | M4/M6, notas públicas do addon; W rating pessoal é recurso diferente. |
| **#366** `docs/issues/mapa.json:7635` — Hide watched discovery | aberta/2.1 | W5 melhora prova; filtro/setting na descoberta é entrega própria. Indicador de biblioteca lançado não prova filtro Home. |
| **#280/#365** `docs/issues/mapa.json:5822`,7614 | precisa-log/aberta,2.0.6 | Catálogos/fileiras, regressões C/J3; não consequência automática de watch_state. |
| **#391/#451/#404/#405** `docs/issues/mapa.json:8087`,9113,8346,8378 | aberta/2.0.6 | Episódios/CW/busca, investigar bugs antes da ampliação. Importador novo não fecha bug pré-existente sem reprodução. |
| **#349/#434/#337/#374** `docs/issues/mapa.json:7266`,8963,6999,7813 | aberta/2.1 | Preferência MP4/P2P, esconder gêneros, esconder skip, sync por linha são ações específicas; extensão de dados só ajuda, não as implementa. |
| **#348** `docs/issues/mapa.json:7245` | aberta/futuro | “Dots y imdb” ambígua; sem ligação/fechamento inventado. |
| **#397/#394/#398/#462** `docs/issues/mapa.json:8196` (#397), `:8155` (#394), `:8217` (#398), `:9233` (#462) | respondida/abertas | Codec/ASS/contorno/AutoSync têm causas por plataforma; hints/flags não prometem decodificação/render corretos. |
| **#326/#329/#331/#352/#355/#356/#358/#362/#383/#425/#429/#430/#433** | GitHub aberta, mapa indica lançada; alguns parciais | São reuso/regressão, não novos fechamentos em massa. #352 mantém árabe/limites pendentes; #433 mantém confirmação de formato/arte. Notas em `docs/issues/mapa.json` prevalecem sobre roteiro histórico. |

Outros pedidos addon/tracking/metadata já fechados (contexto, não reabrir): #37 novos tipos, #83/#277 addons, #110 Simkl CW, #179 Trakt scrobbling, #194 TMDB collections, #254 sync fileiras, #360 sync addons, #400 ordem do addon. O novo adaptador não substitui essas entregas nem pede migração de credenciais existentes.

## 16. Testes futuros — contra o quê e com qual evidência

### 16.1 Offline primeiro, menor harness que falha com a regressão

Não rodados neste trabalho. Reusar assert/stubs existentes e criar só testes dos novos ramos.

| Contrato / regressão | Casos mínimos, resultado esperado | Base reutilizável |
|---|---|---|
| Manifesto | push-only/pull-only/string/objeto, ausência versus events vazio, evento desconhecido, prefixos meta ou vídeo, schema inválido; nenhum POST antes de consentimento | `tests/addonurl.c`, `tests/manifesto_cache.c`, `tests/metaprov.c` |
| Transições | start/pause/start/stop; pausa 20 min um evento; buffering/seek/reconexão não pause; stop 0/79/85/89% não some por regra Trakt | `tests/trakt_scrobble.c`, novo `tests/watchstate.c` |
| Prova | trailer tardio, clipe erro 30 s, duração suspeita, seek ao fim, canal/simulação PC → zero falso visto | `tests/player_visto_trailer.c`, `tests/player_regression.c`, `tests/player_retido.c` |
| Journal | retry body/id/at idênticos; crash antes POST/depois aceite antes ACK; disco/rename falham; cheio não apaga stop; ACK antigo não quita gesto novo | `tests/contapend.c`, `tests/syncprog.c`, novo fake do W |
| HTTP | 200/204 vazios sucesso; 401/403 hold; 429 Retry-After segundos/date; timeout/5xx retry; outro 4xx descarta; 3xx não vaza credencial | `tests/rede_pedido.c`, `tests/redeurl_log.c`, `tests/rede_userinfo_log.c` |
| Pessoas | request lento+troca conta/perfil; mesmo addon herdado não escreve owner; viewer rename/colisão/404; remoção/reinstalação não reatribui backlog | `tests/syncprog.c`, `tests/jellyfin.c`, `tests/perfilcont.c` |
| Pull | since igual mas items diferente; watched/watchlist/ratings/drop ausente≠vazio; parse parcial inválido não apaga snapshot; membership A removido não apaga local/B | `tests/cw205_diagnostico.c`, `tests/cwlocal.c`, `tests/vistonao.c`, novo fake W |
| IDs/runtime | IMDb meta + Kitsu video; season null/0/1 diferentes; percent-only sem duração mantém desconhecido; arrays watched sem timestamp não vencem tombstone; rewatch recente não concluído por watch velho | `tests/episodiosdup.c`, `tests/progresso.c`, `tests/syncprog.c`, novo fake W |
| Metadata | fields raiz depois de people/videos/_providerArt; IDs numeric/string; IMDb8.3 versus critic83 versus rating pessoal8.3; season poster/name; zero nota/presença | `tests/collections_data.c`, `tests/detail_notas.c`, `tests/notasfontes.c` |
| Coleções | bloco vazio no catálogo; items-only/source-only/mixed/nested/cycle; source escondida/ausente; movie+series; catalogId reescrito; páginas20/50/repetida/vazia; primeira ocorrência vence | `tests/colecoes.c`, `tests/collections_data.c`, `tests/colecao_exibicao.c` |
| Streams | URL+hash+índice, mesmo hash arquivo distinto, header diferente, provider falha mantém alternativa; hints contrariando texto; zero/false≠ausente;1228s e folderSize≠videoSize | `tests/stream_parser.c`, `tests/streamfit_parser.c`, `tests/fonte_ordem_addon.c`, `tests/fontesmortas.c` |
| Subtitles | label/release e flags false; lang com sufixo; anexos antes URL não sequestram vídeo; A→B não vaza; merge lista cheia preserva seleção; ASS sem extensão/VTT/charset; header/Range completos | `tests/addons_legendas.c`, `tests/addons_legendas_merge.c`, `tests/legenda_ass.c`, `tests/legmem_383.c` |
| Jellyfin | sem aiostreams fallback; feature version desconhecida não habilita indevidamente; >6 libs, folder/BoxSet/genre None; >8 fontes, Id inicial=item, versão estável e refresh; CDN final preserva sessão sem token vazado | `tests/jellyfin.c`, `tests/jellyfin_server.py`, `tests/emby.c` |

**Runners existentes a considerar na fase de código, não executados aqui:** `bash tests/addonurl.sh`, `bash tests/manifesto_cache.sh`, `bash tests/trakt_scrobble.sh`, `bash tests/contapend.sh`, `bash tests/syncprog.sh`, `bash tests/player_visto_trailer.sh`, `bash tests/stream_parser.sh`, `bash tests/streamfit_parser.sh`, `bash tests/addons_legendas.sh`, `bash tests/addons_legendas_merge.sh`, `bash tests/colecoes.sh`, `bash tests/collections.sh`, `bash tests/jellyfin.sh`, `bash tests/intro.sh`. Usar sanitizer que cada runner suporta; não alegar todos compatíveis. Runners antigos podem escrever em `/tmp`; revisar saída isolada antes da execução, sem editar agora. Tests J usam servidor fake local; nenhuma instância externa é necessária para red→green.

### 16.2 Instâncias externas somente com autorização futura

1. **Addon fake offline** que implementa W v2 e injeta status: prova exata de HTTP/idempotência é o primeiro gate.
2. **AIOMetadata configurado como addon**, instalado no Nuvio: coletar manifesto sanitizado para saber se write é playback v1 ou watch_state v2; reporting “When playback starts and stops”, contas de teste Simkl/MDBList/AniList/MAL conforme objetivo e switches de media/watch tracking. Validar pause→resume no outro cliente, stop real e mark manual; conta anime não significa automaticamente suporte a progress/history. Sem usar conta pessoal do dono para testes destrutivos.
3. **AIOStreams Jellyfin com AIOMetadata instalado:** handoff habilitado conforme versão real (`WATCH_STATE_HANDOFF_ENABLED` em A; report/pull switches W). Nuvio conectado por J testa pipeline servidor→addon; Nuvio como cliente addon direto testa W, **não ambos no mesmo play**. Capturar manifesto/Push/Pull/Public/Views/PlaybackInfo e versões sanitizados.
4. **AIOMetadata Jellyfin direto**, stream addon AIOStreams configurado: >6 bibliotecas movie/series/anime/coleções, genreRequired, usuário separado e catálogo tagged. Reproduzir #468 na LG sem inferir IDs virtuais; conferir source picker/redirect/external subtitles/tempo real contra versão do serviço.
5. **Jellyfin real padrão** sem objeto AIO e **Emby**: guard de fallback; caminhos compartilhados não podem quebrar para acomodar servidor virtual.

Registrar versões de Nuvio/addon/servidor/manifesto, plataforma/modelo, sequência, contagem esperada/observada de eventos e status/latência. Fixtures removem tokens, install URL/config ID privado, viewer pessoal, paths/nomes sensíveis; não usar logs contendo credenciais como anexos de issue.

### 16.3 Prova física por plataforma

LG (incluindo webOS antigo), TPK40 e TPK60/65/8–9, Android Media3: tocar→pausar→background→voltar→stop; Home retido→resume; próximo episódio; troca de fonte; rede cai/retry/restart; sem falso evento de trailer. Medir fila/status e não bloquear UI/lifecycle. WGT/VIDAA só promover após POST cancelável/deadline, status+Retry-After e IndexedDB confirmado; CORS/HTTPS e st/mt também. **Nenhuma dessas provas ocorreu nesta pesquisa.**

## 17. Perguntas abertas para o dono

1. Aprova antecipar **só push W v2** de #466 para canário 2.0.6, mantendo pull/expansões em 2.1/futuro e bugs prioritários do patch? Ou prefere todo W em 2.1?
2. **Played:** manter limiar W90, alinhar AIOMetadata80 por configuração explícita ou exigir a mesma regra local por créditos/prova? O campo wire não negocia threshold; precisamos escolher sem falsificar posição.
3. Em Home retido, sessão lógica termina (stop e start novo ao reabrir, recomendado) ou pausa (pause/start com mesmo início)? E pausa de sistema background conta como pause, stop ou checkpoint local até retomada?
4. Primeiro modo aceita um addon/provider por perfil e opt-in local, sem herdar tracking do principal? Quem pode associar viewer/aliases e compartilhar histórico?
5. Preferência de leitura: addon escolhido, Trakt/Simkl atual, conta Nuvio ou union apenas CW? Nunca union destrutivo de watched sem fonte completa; qual destino deve prevalecer em conflito?
6. Para **#459/#467/#438**, delegação por AIOMetadata é resultado aceitável ou autor/dono exige integração direta com Simkl/AniList/MDBList? Quais operações mínimas: pausa, visto, anime status, nota, watchlist?
7. Quais instâncias/versões/configurações de teste podem fornecer cópias sanitizadas e contas descartáveis? A ausência de schema de media info/auth/config não será preenchida por palpite.
8. **Coleções:** primeiro sources-only no Ver Tudo atual, ou items+nested/união já é obrigatório? Coordenação com redesign Biblioteca/Coleções e roadmap servidores locais 2.2?
9. Pode o primeiro lançamento declarar WGT/VIDAA indisponível para entrega durável enquanto a paridade web é validada? Sem isso, o escopo sobe para L e não cabe como “um POST”.
10. Fila privada: qual limite/retenção, comportamento ao desligar/remover addon e consentimento para descartar backlog? Reconectar não pode converter eventos antigos em ações de outra pessoa.
11. Quer tela de ratings pessoais/drop/histórico/export e login PIN específico, ou apenas compatibilidade de dados nos fluxos atuais? Sem pedido concreto, esses itens ficam no futuro.

## 18. Fora do escopo e critérios de qualidade

- Nenhuma API AniList/Simkl nova adivinhada; nenhuma migração OAuth/Vault, config remota, schema D1/conta Nuvio ou deploy.
- Nenhuma promessa DV/codec/passthrough pela presença de metadata; nenhum CSS remoto na UI nativa; nenhum proxy aberto de vídeo/escrita.
- Nenhuma reimplementação de séries, coleções da conta, saga TMDB, relacionados de séries, Trakt, Simkl CW, legenda overlay ou IntroDB/AniSkip já existentes.
- Parser ancorado/limites, ausência≠vazio/false/zero/null, contexto/pessoa congelados, ACK revisado e segredos fora dos logs são obrigatórios mesmo na menor fatia.
- Este arquivo é a única entrega. Issues/releases/alvos acima são propostas; `mapa.json` e GitHub permanecem intactos. Uma implementação futura deve parar no slice aprovado, provar os gates e só então discutir expansão.
