# PLANO-ECOSSISTEMA — sinergia com addons e serviços, de ponta a ponta

**Data:** 12/10/2026. **Worktree:** `/Volumes/ExternalSSD/nv-205-ecossistema`, branch `agente/205-ecossistema`.
**Base examinada:** `84c19b1d` (inclui PLANO-MERCADO e PLANO-SKIP).
**Pergunta do dono (literal):** “o que mais do mundo dos addons e servicos tipo aiostream, aiomanager, xperience etc conseguimos ter uma sinergia para que o app seja magico de uma ponta a outra?”

> Pesquisa e proposta, não implementação. Nenhum código, mapa ou configuração foi alterado. **SUPOSIÇÃO** marca inferência nossa, estimativa ou fato que não conferimos na fonte primária. Portes S/M/L são relativos (S: um arquivo/parser; M: vários pontos + estado/teste; L: tela/fluxo/transporte novos ou várias plataformas), não dias.
>
> **Não refazer:** watch_state, extensões AIOStreams de metadata/collections/streams/subtitles e Jellyfin estão em `docs/issues/PLANO-MERCADO.md` (§4–§11, roadmap §14). Intro/recap/créditos estão em `docs/issues/PLANO-SKIP.md`. Aqui só se referencia.

## 1. Resposta curta — ponytail full

O app já conversa com muito do ecossistema (conta Nuvio, coleções/selos Xperience, debrid no aparelho, Trakt/Simkl, MDBList para notas, legendas por hash). O que falta para ficar “mágico” não é mais um parser de título: são **as pontas** — entrar e trazer a configuração da pessoa sem digitar nada, e devolver o que ela assistiu para os serviços que ela já usa.

Em ordem de valor/custo:

1. **Já funciona sem código — documentar e testar, não construir.** AIOManager, Xperience e o “Nuvio Configurator” escrevem na **conta Nuvio**; nossa TV já puxa addons, coleções, Home e selos dessa conta (§3). Um guia “configure no celular, a TV segue” entrega metade da mágica.
2. **Vincular por QR/código em vez de digitar chave.** RD, TorBox, Premiumize, AllDebrid, Trakt, Simkl e MDBList têm fluxo device-code/PIN; Trakt e Simkl já usamos. Um laço “mostra código, consulta” reaproveitável (M).
3. **Estado da conta debrid antes do play** (premium/expiração) para RD/Premiumize/TorBox, como já fazemos com AllDebrid (S).
4. **Simkl scrobble start/pause/stop** — o contrato existe no blueprint oficial; fecha #459 sem esperar watch_state (S–M).
5. **Instalar addon/coleção por URL e deep link** (`stremio://`, `nuvio://`, URL de `nuvio-collections.json`) e respeitar `configurationRequired` com QR para configurar no celular. Depende de decisão do dono sobre a mão única #360 (§7, P1).
6. **Importar do Stremio** (QR `link.stremio.com` → `addonCollectionGet`; depois biblioteca/progresso por `datastoreGet`). API não documentada: só leitura, com aprovação do dono (M; L com progresso).
7. **Não fazer:** plugins que raspam sites (CloudStream #437), escrever configuração do AIOStreams pela TV, OAuth Letterboxd, `getSkipGaps` (premium Stremio), checagem instantânea RD (endpoint desativado).

**Concorrência a ter em mente:** o AIOStreams agora tem app próprio para LG/Samsung (webOS 23+/Tizen 7+, “testado só em emulador”) e o Nuvio oficial tem `NuvioTVSmart` (webOS/Tizen). Nosso espaço é TV antiga e nativo rápido; sinergia com a **conta Nuvio** é o que nos torna intercambiáveis com esse mundo. Fontes no §2.

## 2. Fontes e limites

Pesquisa em 12/10/2026 em documentação e repositórios públicos. Nada autenticado, nenhuma conta real usada.

| Tema | Fonte primária |
|---|---|
| AIOStreams (docs completas em texto) | https://docs.aiostreams.viren070.me/llms.txt, `/llms-full.txt`; apps: https://docs.aiostreams.viren070.me/apps; repo https://github.com/Viren070/AIOStreams (AGPL-3.0, v2.36.0); TV: https://github.com/Viren070/AIOStreams/releases/tag/smart-tv |
| AIOManager + protocolo Hydra | https://github.com/Sonicx161/AIOManager (MIT, v2.0.0 24/09/2026), `content/docs/api/hydra.mdx`, `server/providers/nuvio-driver.js` |
| Xperience | https://xperience-app.com/, https://xperience-app.com/guide (fechado, sem API pública) |
| AIOMetadata | https://github.com/cedya77/aiometadata (GPL-3.0, v3.4.1), `addon/lib/collectionExportRoutes.js`, `addon/lib/collectionBuilder/types.ts` |
| StremThru | https://github.com/MunifTanjim/stremthru (MIT), https://docs.stremthru.13377001.xyz |
| Torrentio / Comet / MediaFusion | https://github.com/TheBeastLT/torrentio-scraper, https://github.com/g0ldyy/comet, https://github.com/mhdzumair/MediaFusion |
| Catálogo de addons | https://docs.stremio-addons.net/developers/api; `https://v3-cinemeta.strem.io/addon_catalog/all/community.json` |
| Nuvio oficial | https://github.com/NuvioMedia (NuvioTV `core/deeplink/DeepLinkParser.kt`; self-host `docs/client-configuration.md`) |
| Stremio core/API | https://github.com/Stremio/stremio-core `src/types/api/request.rs` (conferido: `AddonCollectionGet/Set` :58-66, `SkipGaps` :91, `LinkRequest` :267-288, `DatastoreCommand` :303-332; `LINK_API_URL` em `src/constants.rs:90-91`) |
| Debrid | https://api.real-debrid.com/, https://api-docs.torbox.app/, https://www.premiumize.me/api, https://docs.alldebrid.com/, https://debrid-link.com/api_doc/v2/authentication, https://github.com/Offcloud/offcloud-api |
| Legendas | https://opensubtitles.stoplight.io/docs/opensubtitles-api, https://subdl.com/api-doc, https://subsource.net/api-docs, https://docs.wyzie.io/subs/usage/direct, https://github.com/xtremexq/StremioSubMaker |
| Trackers/listas | https://trakt.docs.apiary.io/, https://simkl.docs.apiary.io/ (blueprint conferido: scrobble em `simkl.apib:2932-3277`), https://api.mdblist.com/docs/authentication/, https://docs.anilist.co/, https://myanimelist.net/apiconfig/references/authorization, https://letterboxd.com/api-beta/, https://publicmetadb.com/api-docs |
| Pôsteres | https://rpdb.docs.apiary.io/, https://api.top-posters.com/api |

**Limites:** números de cota de SubDL/SubSource/Wyzie/MDBList/RPDB, limites gratuitos novos da Trakt e campos de expiração da TorBox vieram de trechos de busca — **SUPOSIÇÃO** até fixture. Quais addons emitem `clientResolve` do Nuvio: **SUPOSIÇÃO**. API do Stremio (`api.strem.io`, `link.stremio.com`) é pública mas **sem política de uso para terceiros**.

**Privacidade (regra deste plano e de quem implementar):** URL de configuração de addon é segredo. AIOStreams leva `<uuid>/<senha-cifrada>`, AIOMetadata `<uuid>/<config-comprimida>`, Comet base64 com chave debrid, Torrentio a chave debrid em texto puro, Xperience um link assinado pessoal. Nunca registrar em log, telemetria, relatório de log, captura ou commit; mascarar como `debrid_chave_mascarada` já faz (`src/debrid.h:40`). Exemplos aqui usam `<config>`, `<uuid>`, `<token>`.

## 3. Mapa do ecossistema 2026 — o que um cliente consegue consumir

| Serviço | O que é | O que o cliente consome | Nossa situação |
|---|---|---|---|
| **AIOStreams** | Agregador de fontes/legendas/metadados; agora também app (web, Android TV, desktop, LG/Samsung) | Wire Stremio + extensões (PLANO-MERCADO); `GET /api/v1/status` público; `GET /api/v1/search` com campos já analisados (auth Basic `<uuid>:<senha>`); `GET /api/v1/anime` sem auth, mapeia AniList/MAL/Kitsu/TMDB/TVDB/IMDb/Trakt/Simkl; templates `…/stremio/configure?template=<url>`; alias `…/stremio/u/<alias>/manifest.json`; variantes `/v/<id>` | Lemos `clientResolve.isCached` e marcadores de texto (`src/stream_parse.c:215-224`, `:374-381`). Resto em PLANO-MERCADO. |
| **AIOManager** | Gerenciador web de contas/addons Stremio e **Nuvio**; ordem/saúde/backup entre contas | Escreve na conta Nuvio por `sync_push_addons`; API Hydra (`X-API-Key`): `/hydra/addons`, `/hydra/version` (polling barato), `/hydra/addons/{url}/health`, `/hydra/register` | **Zero código já:** TV puxa `addons` da conta (`src/sync.c:409-476`). **SUPOSIÇÃO:** mesmo backend (RPCs coincidem com `src/sessao.c:467,538`). |
| **Xperience** | Construtor visual pago/fechado de catálogos, coleções e Home para Nuvio | Addon com JWT na URL; “Push to Nuvio” na conta ou `collections.json`; pacotes de selos | Já reconhecido: addon sem stream (`src/addons.c:44-47`), coleções/arte (`src/colecoes.c:606-704`, `:709-711`), selos embutidos (`src/selospacote.h:3-27`). |
| **Nuvio Configurator** (`nuvio.tvflix.co.uk`, #414) | SPA de terceiros que gera `nuvio_collections.json`, `aio_meta.json` e config AIOStreams | Arquivos para importar na conta/app oficial | Coleções chegam se a pessoa importar na conta. Importar arquivo/URL direto na TV: lacuna. |
| **AIOMetadata** | Metadados/arte/catálogos/listas MDBList/AniList/MAL/Trakt; modo Jellyfin | `/stremio/<uuid>/nuvio-collections.json` e `/fusion-widgets.json`; `app_extras` na meta | Lemos o mesmo esquema de coleção da conta (`heroBackdropUrl`, `titleLogoUrl`, `focusGifUrl`: `src/colecoes.c:620-643`). `app_extras` não lido. |
| **StremThru** | Camada única sobre 10 debrids + proxy + listas + `sidekick` | `/v0/store/user`, `/v0/store/torz/check?hash=…` (cache unificado com `video_hash`), `/v0/meta/id-map/{movie|show}/{imdb}` | Nada. Útil só se a pessoa tiver instância. |
| **Torrentio / Comet / MediaFusion** | Addons de torrent/debrid | `bingeGroup`, `filename`, `videoSize` comuns; MediaFusion manda `notWebReady`/`proxyHeaders`; Comet ⚡/⬇️ | Lemos bingeGroup/videoHash/proxyHeaders (`src/stream_parse.c:342-350`), seeders (`:266-283`). |
| **stremio-addons.net** | Catálogo comunitário (≈634 addons) | `GET /api/v0/addons`, `/categories`, `/rising`; manifest, `configureUrl` | Nada. **ToS:** atribuição e proibição de uso “concorrente” sem permissão escrita. |
| **Cinemeta `addon_catalog`** | Catálogo oficial Stremio | `…/addon_catalog/all/community.json` (96) e `/official.json` (7) | Nada. Público, sem chave. |
| **Stremio conta** | Conta oficial | `link.stremio.com/api/v2/create` → `{code, link, qrcode}`, `/read?code=` → `authKey`; `addonCollectionGet`; `datastoreGet/Put` coleção `libraryItem` (`state.timeOffset`, `flaggedWatched`, `_mtime`) | Nada (sem `api.strem.io` no código). |
| **Debrid** | RD, TorBox, Premiumize, AllDebrid, Debrid-Link, Offcloud, Easynews | Device-code/PIN: RD, TorBox, PM, AD, Debrid-Link. Expiração: RD `/user`, PM `/account/info`, AD `/v4/user`, TorBox `/user/me` (**SUPOSIÇÃO** nos campos). Cache: TorBox/PM/Offcloud sim; RD `instantAvailability` **desativado** (erro 37); AD sem instant | Resolvemos no aparelho RD/TB/PM/AD (`src/debrid.c:492-1020`); chave digitada ou da conta (`src/sync.c:586-592`); só AD tem teste de conta (`src/debrid.c:994-1020`). |
| **Legendas** | OpenSubtitles REST v1, addon OpenSubtitles v3, SubDL, SubSource, Wyzie, SubMaker (agregador + tradução IA) | Hash/tamanho/nome; `foreign_parts_only` (forçada); `hearing_impaired`; cotas por chave | Só via addon; mandamos `videoHash/videoSize/filename` e medimos hash por Range (`src/addons.c:1222-1264`, `src/legextras.h:1-14`). |
| **Trackers/listas** | Trakt, Simkl, MDBList, AniList, MAL, PublicMetaDB, Letterboxd | Trakt/Simkl/MDBList device/PIN + scrobble; AniList/MAL OAuth por navegador; PublicMetaDB chave colada; Letterboxd API fechada | Trakt completo (`src/trakt.c`, `src/traktscrobble.h`); Simkl sem scrobble (`src/simkl.h:47-48`); MDBList só notas (`src/extras.c:689-714`). |
| **Nuvio oficial (extensões)** | Campos além do Stremio | Stream `clientResolve{service,isCached,stream.raw.parsed…}`; meta `app_extras`, `landscapePoster`, `trailerStreams`; catálogo `showInHome`, `pageSize`; manifest `configVersion`, `stremioAddonsConfig`; deep links `nuvio://…`; `<backend>/.well-known/nuvio` | Lemos `clientResolve` parcial e `landscapePoster` (`src/home.c:6778`). Demais: lacuna. |
| **Stremio core novo** | Fontes além de URL | `externalUrl` + `androidTvUrl`/`tizenUrl`/`webosUrl`; `nzbUrl(s)`, `rarUrls`…; `behaviorHints.notWebReady`, `countryWhitelist` | `externalUrl` vira URL tocável (`src/stream_parse.c:299`) — ver risco J5. Resto: nada. |

## 4. Já funciona hoje — sinergias sem código

Validar com conta de teste e escrever um guia curto (Discord/README, em inglês). Nada disso exige build.

| Fluxo | Caminho | Evidência | A validar |
|---|---|---|---|
| Gerenciar addons no AIOManager | AIOManager → conta Nuvio → `sync_pull` da TV | `src/sync.c:409-476`, botão “Sync addons” `src/addonsui.c:61-69` | **SUPOSIÇÃO:** driver Nuvio do AIOManager grava na mesma tabela; ordem `sort_order` respeitada; duplicatas (#360). |
| Home/coleções do Xperience ou AIOMetadata | “Push to Nuvio”/import na conta → `sync_pull_collections` | `src/sync.c:998`, `src/colecoes.c:903-960` | Coleções com >64 entradas já corrigidas (#233). |
| Selos Fusion/Xperience | Pacote na conta (`stream_badge_rules`) ou URL na TV | `src/selospacote.h:3-9` | Limite 3 pacotes. |
| Fonte cacheada do AIOStreams | `clientResolve.isCached:false` derruba no autoplay | `src/stream_parse.c:374-381`, `src/debrid.h:16-19` | — |
| Legendas casadas por hash | OpenSubtitles v3/SubMaker com `videoHash` | `src/addons.c:1222-1264`, `src/legauto.c:93` | Recomendar SubMaker no guia (SubDL/SubSource sem código nosso). |

## 5. A jornada mágica — etapa por etapa

Cada etapa: **Sinergia** (serviço/protocolo) · **Temos** (`arquivo:linha`) · **Lacuna** · **Porte/risco** · **Issues**.

### J1 — Primeira abertura / onboarding

- **Sinergia:** login Nuvio por QR (já) + “o que você já usa?”: conta Nuvio com addons (AIOManager/Xperience), conta Stremio (QR `link.stremio.com`), ou pacote inicial. Vincular debrid e tracker na mesma sequência, todos por código.
- **Temos:** QR de sessão TV `src/login.c:182-226`, `src/sessao.c:467,538`; QR próprio byte-mode v1–6, até 134 bytes (`src/qr.h:1-37`); página LAN de digitação pelo celular (`src/celular.h:1-52`); perfis/PIN (`src/perfis.c:54-436`); idioma da conta/TV (`src/idiomaauto.h:1-12`); Trakt device code (`src/traktauth.h:1-14`) e Simkl PIN (`src/simklauth.h:1-12`) só em Ajustes (`src/ajustes.c:7076-7077`).
- **Lacuna:** não há assistente de primeira abertura (`src/abertura.h` é só abertura visual). Conta vazia cai numa Home vazia sem caminho. Sem login Stremio.
- **Proposta mínima:** com conta sem addons, a Home vazia mostra três cartões: “Configurar no celular” (QR para a página de addons da conta Nuvio/AIOManager), “Importar do Stremio” (J2b), “Vincular debrid/Trakt” (QR). Sem tela nova de wizard; reaproveitar o estado vazio. **SUPOSIÇÃO** de que estado vazio cobre o caso.
- **Porte/risco:** S–M / baixo. Corrigir antes #418 (QR não gera / servidor não responde) — bug na porta de entrada.
- **Issues:** #418 (pré-requisito, não fechado por isso), #20 (contexto).

### J2 — Configurar e importar addons

**J2a — Instalar por URL, deep link, configurar.**
- **Sinergia:** `stremio://<host>/…/manifest.json`, `https://web.stremio.com/#/addons?addon=<enc>`, `nuvio://<host>/…/manifest.json` (aceitos pelo NuvioTV); `behaviorHints.configurable`/`configurationRequired`; templates AIOStreams; alias AIOStreams `…/u/<alias>/manifest.json` (curto, cabe em QR).
- **Temos:** normalização `stremio://`→`https://` e base com query (`src/addonurl.h:139-164`), apelido para URL > 2048 (`:166-295`), `addons_adicionar` (`src/addons.c:1432-1476`) chamado só pela lista curada da TV ao vivo (`src/guia.c:1971-1995`); manifesto/resources (`src/addons.c:1630-1729`).
- **Lacuna:** nenhuma tela para colar/receber URL; `configurable`/`configurationRequired` nunca lidos (zero ocorrências); nenhum handler de deep link de addon. **Conflito de produto:** a lista é mão única desde #360 (`src/sync.h:13-18`) — instalar na TV some no próximo pull.
- **Proposta mínima (depende do dono, §8 Q1):** (a) receber URL pela página do celular (`celular_abrir`) e **gravar na conta** por inserção individual (não o push total que causou #360), ou (b) só QR “abra no celular” para a página de addons da conta. Para addon com `configurationRequired`, mostrar QR do `configureUrl`/página do celular em vez de instalar quebrado. **SUPOSIÇÃO:** #414 (“Okay to load” que não faz nada) é addon que exige configuração; confirmar com o autor.
- **Porte/risco:** (b) S/baixo; (a) M/médio (duplicatas, segredo na URL).
- **Issues:** #414 (provável), #360 (decisão), #437 (não — ver §7).

**J2b — Importar do Stremio.**
- **Sinergia:** `GET link.stremio.com/api/v2/create?type=Create` → `{code, link, qrcode}`; poll `/api/v2/read?type=Read&code=<code>` → `authKey`; `POST api.strem.io/api/addonCollectionGet {type:"AddonCollectionGet", authKey:<token>, update:true}`.
- **Temos:** nada (`api.strem.io` ausente em `src/`). Escrita na conta Nuvio para addons foi removida (#360).
- **Lacuna/Proposta:** importação **única e só leitura** da coleção Stremio para a conta Nuvio do perfil, descartando addons oficiais duplicados (Cinemeta etc.), com prévia antes. `authKey` só em memória, nunca na conta Nuvio nem no log.
- **Porte/risco:** M / médio-alto (API sem política para terceiros; URLs com segredo).
- **Issues:** nenhuma aberta pede isso explicitamente — **SUPOSIÇÃO** de demanda; confirmar com o dono/Discord.

**J2c — Saúde do addon.**
- **Sinergia:** AIOManager `/hydra/addons/{url}/health`; AIOStreams `/api/v1/status`.
- **Temos:** latência por TV e “silencioso/lento” (`src/addonstats.h:1-62`), motivo de vazio e addon fora do ar (`src/addons.c:2157`, `:2557`). Já melhor que a sinergia externa.
- **Lacuna:** nenhuma que justifique código. **Fica.**

### J3 — Descoberta e catálogos

- **Sinergia:** coleções no formato Nuvio vindas de AIOMetadata/Xperience/Configurator por **URL** (`/stremio/<uuid>/nuvio-collections.json`); listas MDBList/AniList/Letterboxd como catálogos via AIOMetadata ou StremThru `/stremio/list` (zero código: são addons); `extra.options` de gênero do manifesto; `showInHome`/`pageSize` do catálogo Nuvio; Cinemeta `addon_catalog` para descobrir addons.
- **Temos:** coleções com fontes addon/TMDB/Trakt (`src/colecoes.h:19-31`), Home da conta (`src/sync.c:964`), busca agregada (`src/descoberta.c:897`, `src/descoberta.h:166-187`), paginação `skip`/`genre` (`src/descoberta.c:8032-8033`), listas Trakt/Simkl (`src/listas.c:547-695`), metadados `catalog.nuvio.tv`→Cinemeta (`src/metaprov.h:1-13`), social (`src/recomenda.h:1-13`).
- **Lacunas:** (1) importar coleção por URL na TV — reaproveita o parser da conta (`src/colecoes.c:903-960`), mesmo esquema (`types.ts` do AIOMetadata) — S–M; (2) `extra.options` de gênero não lido (`src/vertudo.c:419` só por coleção) — S; (3) `showInHome` ignorado — S, ajuda #358/#365 (**SUPOSIÇÃO**); (4) navegar addons pelo Cinemeta `addon_catalog` — M, valor baixo frente a J2b.
- **Risco:** baixo (1–3); URL de coleção do AIOMetadata contém `<uuid>` — segredo.
- **Issues:** #414 (parcial), #438 (listas MDBList já possíveis via addon — responder com o caminho), #365/#280 (só se causa for `showInHome`, provar antes), #405 (não relacionado — bug próprio).

### J4 — Página do título

- **Sinergia:** notas agregadas MDBList; `app_extras` (elenco com `tmdbId`, certificação, datas) e `trailerStreams`/`trailers[].ytId` da meta Nuvio/AIOMetadata; pôsteres RPDB/TOP Posters/aioratings; onde assistir TMDB.
- **Temos:** notas MDBList em vários provedores (`src/extras.c:35-40`, `:689-714`; `src/notasfontes.c:41-179`); pôsteres RPDB/SpatialPosters/modelo de URL (`src/posterprov.h:1-14`); trailer Apple→IMDb→YouTube (`src/trailerfonte.h:1-14`); onde assistir (`src/ondever.c:264`).
- **Lacuna:** `app_extras` e `trailerStreams` não lidos (zero ocorrências). Notas/arte da meta do addon já estão em PLANO-MERCADO M1–M4/M7 — **não repetir**; só acrescentar `app_extras` como fonte alternativa de elenco/certificação. TOP Posters/aioratings cabem no “modelo de URL” existente: só documentar.
- **Porte/risco:** S / baixo.
- **Issues:** #406 (via MERCADO M4), #189 (contexto, fechada).

### J5 — Escolher a fonte

- **Sinergia:** `clientResolve.service`/`isCached`/`stream.raw.parsed` (Nuvio/AIOStreams); checagem de cache TorBox `checkcached` e Premiumize `cache/check` (já usamos), StremThru `torz/check` (unificado); estado de conta debrid; `notWebReady` (MediaFusion marca P2P/proxy); `externalUrl` + `tizenUrl`/`webosUrl`/`androidTvUrl`; regras regex/SEL já espelhadas.
- **Temos:** extração completa (`src/stream_parse.c:286-409`); cache/não cache por texto (`:215-224`) e `isCached` (`:374-381`); resolução RD/TB/PM/AD no aparelho (`src/debrid.c:492-1020`); autoplay só cacheado (`src/debrid.h:16-19`); detecção reativa de conta sem plano (`src/debrid.c:194-229`, `:349`); regras de autoplay (`src/fonteregra.h:1-35`); memória de fonte por bingeGroup (`fontepref.c`); plugins JS Nuvio (`src/plugins.h:1-14`) e P2P (`src/p2p.h:1-20`).
- **Lacunas e propostas:**
  1. **Expiração/premium proativo** RD `/rest/1.0/user` (`expiration`, `premium`), PM `/api/account/info` (`premium_until`), TorBox `/v1/api/user/me` (**SUPOSIÇÃO** nos campos) — copiar `debrid_testar_alldebrid`; aviso “expira em N dias” em Ajustes e no motivo de vazio. **S / baixo.**
  2. **Serviço por fonte** via `clientResolve.service` — já é PLANO-MERCADO S1. **Não repetir.**
  3. **`notWebReady:true` + P2P desligado** → não oferecer nem autoescolher; base para #349. **S / baixo.** **SUPOSIÇÃO:** é a causa de “P2P escolhido com P2P desligado” — provar com fixture.
  4. **`externalUrl` tratado como mídia** (`src/stream_parse.c:299`) — no Stremio é link para abrir fora; tocar dá “não abre a fonte”. Separar e, se houver `webosUrl`/`tizenUrl`/`androidTvUrl`, oferecer abrir no app da plataforma. **S / médio** (pode mudar o que hoje “funciona por acaso”; **SUPOSIÇÃO** de impacto).
  5. **Cache RD:** não existe mais checagem instantânea (erro 37). Manter add-and-poll atual; **não** prometer selo ⚡ próprio para RD.
  6. **Dedup/resumo de fonte** — MERCADO S4/P4 (#458/#457).
- **Issues:** #349 (item 3), #402 (MERCADO), #458/#457 (MERCADO), #364/#53 (contexto).

### J6 — Reprodução, legendas e skip

- **Sinergia:** legendas por hash via addon (OpenSubtitles v3, SubMaker agregando OS/SubDL/SubSource/Wyzie + tradução); OpenSubtitles REST direto (`foreign_parts_only` para forçada, `hearing_impaired`, `fps`); TorBox `createstream` (escolha de faixa no servidor); skip conforme PLANO-SKIP.
- **Temos:** legendas de addons filtradas por idioma até 36 (`src/addons.c:1308-1376`), extras de hash (`src/legextras.h:1-14`), autoescolha por idioma/nome/histórico (`src/legauto.c:93`), SRT/VTT/ASS (`src/legenda.c:651-692`); skip IntroDB/AniSkip/capítulos/créditos (`src/intro.c`, `src/credfonte.c`).
- **Lacuna:** OpenSubtitles REST direto exigiria chave do app, login e cota por usuário (link de download 3 h, sem cache) — custo alto para ganho que addons já dão. Forçada/SDH vêm pela via do addon em MERCADO U1/U2. Sincronização automática não existe em nenhuma API — é local (#374/#462).
- **Proposta:** nenhuma integração nova; recomendar SubMaker no guia; marcar faixa forçada quando o addon informar (MERCADO U2).
- **Porte/risco:** — (zero código) / baixo.
- **Issues:** #374/#462/#394/#398 (fora deste plano), #429/#383 (lançadas).

### J7 — Tracking e Continuar assistindo

- **Sinergia:** Simkl scrobble `/scrobble/start|pause|stop` (≥80% marca visto; um scrobble ativo por título — blueprint `simkl.apib:2932-3277`); Trakt (já); MDBList device-code + scrobble + `/sync/watched` (**SUPOSIÇÃO** nos detalhes); AniList `SaveMediaListEntry` / MAL `my_list_status` por OAuth via QR→navegador; IDs de anime pelo `GET /api/v1/anime` do AIOStreams (sem auth) ou StremThru `id-map`; watch_state delegado (MERCADO); PublicMetaDB (chave colada, retomada por ID).
- **Temos:** Trakt playback/histórico/watchlist/scrobble (`src/trakt.c:1035-2149`, `src/traktscrobble.h:1-20`); Simkl lê pausados e marca visto, sem scrobble (`src/simkl.c:248-729`, `src/simkl.h:47-48`); progresso da conta (`src/syncprog.c:84-208`); visto distribuído (`src/visto.h:1-14`) com fila persistida (`src/contapend.c:392-643`); ordem do CW (`src/cwordem.h:1-14`); reconciliação conta/Trakt/Simkl (PLANO-MERCADO §3).
- **Propostas:**
  1. **Simkl scrobble espelhando o Trakt** no mesmo ponto do player (start/pause/stop com `progress`), respeitando o lock de 20 s do checkin e sem marcar visto em pausa. **S–M / médio** (falso visto). PLANO-MERCADO pedia “não inventar endpoint”: o endpoint agora está **conferido** na fonte primária.
  2. **MDBList** como destino de visto/watchlist só depois de 1 (mesmo laço). **M / médio.** Antes, responder #438 perguntando o que falta (notas e listas via addon já existem).
  3. **AniList/MAL direto** só se a delegação watch_state (MERCADO P7) não atender. Mapear IDs por `/api/v1/anime` em vez de tabela própria. **L / alto.**
- **Issues:** #459 (fecha com 1), #393 (biblioteca Simkl — fonte certa, não só rótulo), #438 (2), #467 (3/MERCADO), #466 (MERCADO), #451/#404 (bugs CW — investigar antes de qualquer nova fonte).

### J8 — Conta e sincronização entre aparelhos

- **Sinergia:** conta Nuvio como **hub** (AIOManager, Xperience, Configurator e o app oficial já escrevem nela); `<backend>/.well-known/nuvio` para quem hospeda backend próprio (`{backend_url, publishable_key, capabilities}`); Hydra como origem alternativa de addons sem conta Nuvio; Stremio `datastoreGet` para trazer biblioteca/progresso (`libraryItem.state.timeOffset`, `flaggedWatched`, `_mtime`).
- **Temos:** RPCs `sync_pull/push_*` de ajustes, credenciais, vistos, biblioteca, coleções, Home e plugins (`src/sync.c`, `src/nuvem.c:8-81`); credenciais por provedor sem Trakt ainda (`src/sync.h:21-25`); Jellyfin/Plex (`src/jellyfin.h`, `src/plex.c`); servidores próprios `servidor/recomendacoes`, `servidor/juntos`, `servidor/tv`.
- **Lacunas/propostas:**
  1. **Trakt na conta** quando o web escrever a linha — já previsto no código (`src/sync.h:21-25`); só acompanhar.
  2. **`.well-known/nuvio`** em vez de `art/nuvem.txt` para self-host — **S / baixo**, nicho; futuro.
  3. **Progresso Stremio → CW** (`datastoreGet`, só leitura, uma origem por perfil) — **L / alto** (conflito com reconciliação existente). Futuro, depois de J2b provar valor.
  4. **Hydra** — futuro; só se houver pedido de quem usa AIOManager sem conta Nuvio.
- **Issues:** #233/#360 (contexto), #451 (bug primeiro).

## 6. Laço único de vínculo por código (base de J1, J5, J7)

Um só componente “mostrar código + QR + consultar até autorizar/expirar”, já existente em forma ad hoc em `src/traktauth.h` e `src/simklauth.h`. Provedores e contratos:

| Provedor | Início | Consulta | Resultado | Observação |
|---|---|---|---|---|
| Real-Debrid | `GET /oauth/v2/device/code?client_id=<client_id open-source>&new_credentials=yes` | `/oauth/v2/device/credentials`, depois `POST /oauth/v2/token` | `access_token` + `refresh_token` | Escopo limitado; chave privada nunca em app público |
| TorBox | `GET /v1/api/user/auth/device/start?app=<nome>` | `POST /v1/api/user/auth/device/token` | token | `verification_url_complete` cabe no QR |
| Premiumize | `POST /token response_type=device_code` | `grant_type=device_code` | token | Exige registrar cliente |
| AllDebrid | `GET /v4.1/pin/get` | `POST /v4/pin/check` | `apikey` | — |
| Trakt / Simkl | já implementados | — | — | Reusar |
| MDBList | `POST /oauth/device-authorization/` | — | token | Exige registrar app (**SUPOSIÇÃO** nos detalhes) |
| Stremio | `link.stremio.com/api/v2/create` | `/api/v2/read` | `authKey` | Não documentado |

Limite do QR: 134 bytes (`src/qr.h`). Todas as URLs de verificação acima cabem; URL de configuração de addon pode não caber — usar a página do celular. Credencial obtida vai para `sync_push_provider_credentials` (mesmo lugar das chaves atuais, `src/sync.c:586-592`). **Porte:** M (componente + 4 provedores); **risco:** médio (refresh/expiração, registro de apps).

## 7. Não fazer (e por quê)

| Ideia | Motivo |
|---|---|
| CloudStream repos (#437) e mais raspadores tipo Nuvio providers | Extensões Kotlin/dex não rodam no nosso C; raspagem de sites piratas é risco legal/ToS. Já temos plugins JS Nuvio (`src/plugins.h`); não ampliar. Sugerir fechar #437 como fora do escopo (dono decide). |
| Escrever configuração do AIOStreams pela TV (`PUT /api/v1/user`) | Substitui o config inteiro; perda de dados e senha na TV. Só QR para a página de configuração. |
| `getSkipGaps`/`seekLog` do Stremio | Exige Stremio premium e manda dados de uso; PLANO-SKIP já tem cadeia própria. |
| Checagem instantânea RD/AllDebrid | Endpoints desativados/inexistentes; selo seria mentira. |
| Letterboxd API | Fechada, só por convite; já temos import do export (`servidor/recomendacoes/src/letterboxd.js`). |
| Navegar stremio-addons.net no app | ToS proíbe uso “concorrente” sem permissão escrita; Cinemeta `addon_catalog` cobre o básico. Só com permissão (§8 Q3). |
| ChillLink, Hydra, StremThru store no 2.0.6 | Valor marginal; ninguém pediu. Futuro sob demanda. |
| OpenSubtitles REST direto | Chave/cota por app e por usuário; addons já entregam (J6). |

## 8. Perguntas ao dono

1. **Mão única dos addons (#360):** a TV pode **adicionar** um addon na conta (inserção individual, nunca push da lista toda), ou a TV só mostra QR para gerenciar fora (conta Nuvio/AIOManager)? Decide J2a.
2. **API do Stremio sem política pública:** autorizar importação só leitura (QR `link.stremio.com` → addons; depois biblioteca)? `authKey` só em memória.
3. **stremio-addons.net:** pedir permissão por escrito ao mantenedor, ou ficar só com Cinemeta `addon_catalog` (ou nenhum catálogo)?
4. **Simkl scrobble (#459) no 2.0.6**, direto, antes do watch_state do PLANO-MERCADO? Contrato confirmado.
5. **Registro de apps** (Premiumize, MDBList, Debrid-Link, eventualmente OpenSubtitles): quem registra, em nome de quem, e os segredos ficam como os do Trakt (defines de build, fora do repositório)?
6. **Deep links (#420):** usar `nuvio://movie/<imdb>` e `nuvio://detail/tv/<imdb>` como o oficial (colide no Android se ambos instalados) ou `nuviolegacy://` com as mesmas formas?
7. **#437 CloudStream:** fechar como fora do escopo com a justificativa do §7?
8. **Guia “configure no celular, a TV segue”** (AIOManager/Xperience/Configurator → conta Nuvio): publicar no Discord em inglês depois de validar com conta de teste?

### Decisões do dono (12/10/2026)

- **Q1:** sim — a TV pode adicionar um addon na conta, um por vez (nunca a lista inteira).
- **Q2:** sim — importação só leitura pela API do Stremio autorizada; `authKey` só em memória.
- **Q4:** sim — Simkl scrobble (#459) entra na 2.0.6, antes do watch_state.
- **Q6:** `nuviolegacy://` com as mesmas formas do oficial (`nuviolegacy://movie/<imdb>`, `nuviolegacy://detail/tv/<imdb>`).
- **Q3:** só Cinemeta `addon_catalog`; não pedir permissão ao stremio-addons.net.
- **Q8:** sim — guia "configure no celular, a TV segue" no Discord, em inglês, depois de validar com conta de teste.
- **Q5:** sem registro de apps por enquanto. MDBList por chave do usuário (conta Nuvio no celular); Premiumize/Debrid-Link seguem por chave colada. Se houver demanda, o dono registra em seu nome (como Trakt), segredo só no Vaultwarden.
- **Q7:** fechar #437 como fora do escopo, com a justificativa do §7 (Kotlin/dex não roda no nosso C, risco legal de raspadores, plugins JS Nuvio já cobrem).

## 9. Prioridade proposta (sem sobrescrever o mapa)

| Alvo | Item | Etapa | Porte/risco | Issues |
|---|---|---|---|---|
| **2.0.6 — zero código** | Validar e publicar guia AIOManager/Xperience/Configurator → conta Nuvio → TV; responder #438 e #414 com o caminho | §4, J3 | S/baixo | #438, #414 (resposta, não fechamento) |
| **2.0.6** | Corrigir #418 (QR/servidor no login) | J1 | S–M/médio | #418 |
| **2.0.6** | Premium/expiração RD, PM, TorBox (copiar teste AD) | J5.1 | S/baixo | — |
| **2.0.6** | `notWebReady`+P2P desligado; `externalUrl` separado de mídia | J5.3–4 | S/baixo–médio | #349 (parcial) |
| **2.0.6 (se Q4 = sim)** | Simkl scrobble start/pause/stop | J7.1 | S–M/médio | #459 |
| **2.0.6** | Estado vazio da Home com QR “configure no celular” | J1, J2a(b) | S/baixo | — |
| **2.1** | Laço único de vínculo por código + RD/TorBox/PM/AD | §6 | M/médio | — |
| **2.1** | Instalar addon/coleção por URL (página do celular), `configurationRequired`→QR, deep link `stremio://`/`nuvio://` de addon | J2a, J3.1 | M/médio | #414, #360 (Q1) |
| **2.1** | Deep link de título Android (`ACTION_VIEW`) | J2a | S–M/baixo | #420 (Q6) |
| **2.1** | Importar addons do Stremio por QR (só leitura) | J2b | M/médio-alto | — (Q2) |
| **2.1** | `extra.options` de gênero, `showInHome`, `app_extras` | J3, J4 | S/baixo | #365/#280 só se provado |
| **2.1** | MDBList device-code + visto/watchlist | J7.2 | M/médio | #438 |
| **Futuro** | Progresso/biblioteca Stremio → CW | J8.3 | L/alto | — |
| **Futuro** | AniList/MAL direto com IDs por `/api/v1/anime` | J7.3 | L/alto | #467 (após MERCADO P7) |
| **Futuro** | `.well-known/nuvio`, Hydra, StremThru `torz/check`, `nzbUrl`/usenet, `tizenUrl`/`webosUrl` | J5, J8 | S–L/médio | — |
| **Fora** | CloudStream/raspadores, escrever config AIOStreams, `getSkipGaps`, Letterboxd API | §7 | — | #437 (Q7) |

**Ordem de dependência:** bugs da porta de entrada (#418) e de CW (#451/#404) antes de novas origens; laço de código (§6) antes de mais provedores; decisão Q1 antes de qualquer instalação pela TV; PLANO-MERCADO P1 (watch_state push) não bloqueia nem é bloqueado por Simkl scrobble — os dois usam os mesmos eventos do player, então implementar Simkl no mesmo ponto onde o Trakt é chamado.

## 10. Testes futuros (para quem implementar)

- **Offline primeiro:** fixtures sanitizadas de `/rest/1.0/user` (RD), `/api/account/info` (PM), `/v1/api/user/me` (TorBox), respostas do Simkl scrobble (`start`/`pause`/`scrobble`), manifesto com `configurationRequired`, stream com `notWebReady`/`externalUrl`, `nuvio-collections.json` com >64 pastas. Cada uma: teste FAIL→PASS no parser.
- **Contas reais só com autorização** do dono e conta de teste; nunca a conta pessoal dele em fixture.
- **Prova na TV:** QR legível a 3 m nas TVs de teste (TCL/C9) para cada URL de verificação; Simkl scrobble conferido no site do Simkl após pausa e após ≥80%.
