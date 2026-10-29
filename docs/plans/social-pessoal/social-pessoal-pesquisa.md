# Social & Pessoal: pesquisa (com links)

Apoio a [social-pessoal-plano.md](social-pessoal-plano.md). Pesquisa feita em 06/10/2026. As citações do plano, como [R3], remetem às seções daqui. "Medido" quer dizer que eu li a página ou o código. "Não verificado" quer dizer que vem de um resumo de busca ou de terceiros.

---

## R0. O app das capturas do dono

**Não identificado.** Buscas por "Lock this series", "Filmography Hunt", "Taste Galaxy", "Couch Buddies", "Colour Year", "Fell Asleep" e "Watch Clock", isoladas e combinadas, não acharam nenhum produto publicado. A hipótese mais provável é que seja um conceito, um mockup de design ou um app muito novo e sem página indexada. O que existe de parecido no mercado:

- **"Stream cheating"** é o nome do problema que a trava de série resolve. O Cornetto fez as "Commitment Rings" (2016, campanha publicitária: o episódio só tocava com os dois anéis na sala) e nunca foi produto. O WatchNest resolve com sala sincronizada ("se ver o E4 exige estar na sala, a tentação some"). Nenhum faz a trava *com perfis da mesma TV* nem o modo "Separados, quem está à frente espera".
  - https://www.mentalfloss.com/article/80757/commitment-rings-keep-your-partner-watching-tv-episodes-without-you
  - https://www.watchnest.party/blog/stream-cheating-watching-ahead-partner
  - https://www.shaneco.com/theloupe/articles-and-news/streaming-infidelity-across-the-us/
- **Achar o resto**: as abas Diary/Stats do Letterboxd (R1), os badges do TV Time (R4) e o Year in Review do Trakt (R2).

**Conclusão:** a trava de série, sobretudo entre perfis da mesma TV, não tem concorrente conhecido. É diferencial.

## R1. Letterboxd

| Tema | O que achei | Fonte |
|---|---|---|
| API oficial | **Só por pedido**, por e-mail para api@letterboxd.com, e eles avisam que não respondem a todos nem garantem acesso. OAuth2 (client credentials para dado público, authorization code para dado do membro). Base `https://api.letterboxd.com/api/v0`. Relatos de meses de espera. A página `letterboxd.com/api-beta` respondeu **403** ao WebFetch, então o conteúdo vem de resumo de busca e de `docs/social-fontes.md`, que diz que a API "não libera para projeto pessoal nem para recomendação" (lido em 02/10) | https://letterboxd.com/api-beta/ · https://api-docs.letterboxd.com/ · https://blog.alexbeals.com/posts/extracting-letterboxd-tokens-with-mitmproxy |
| ToS | Proíbe "robot, spider, scraper ... to access, acquire, copy, or **monitor**" o serviço (termos atualizados em 12/2025, segundo a busca). **A decisão anterior continua: não ler RSS de ninguém** (memória m3, 02/10: "Letterboxd RSS proibido por ToS (robo) -> fora") | https://letterboxd.com/legal/terms-of-use/ |
| **Importar CSV (caminho legal de ida)** | Medido: colunas `LetterboxdURI`, `tmdbID`, `imdbID`, `Title`, `Year`, `Directors`, `Rating` (0,5 a 5, passo 0,5), `Rating10` (1 a 10, convertido), `WatchedDate` (AAAA-MM-DD, **cria entrada de diário**), `Rewatch`, `Tags`, `Review` (texto/HTML). UTF-8, **até 1 MB**, só vírgula. É preciso pelo menos uma das quatro primeiras colunas | https://letterboxd.com/about/importing-data/ |
| **Exportar (caminho legal de volta)** | Em Settings › Data, a pessoa baixa um ZIP com `diary.csv`, `ratings.csv`, `reviews.csv`, `watchlist.csv`, `watched.csv` e as listas. Os CSV trazem nome, ano e **Letterboxd URI, sem tmdb nem imdb**, o que obriga a casar por título e ano. Há ferramentas que fazem esse casamento | https://letterboxd.com/user/exportdata/ · https://www.feadin.eu/en/posts/letterboxd_i_love_you_but_we_need_to_talk_about_your_exports/ · https://github.com/Tetrax-10/letterboxd-csv-imdb-tmdb-mapper · https://docs.movary.org/features/letterboxd/ |
| Estatísticas | Pro/Patron têm stats anuais e de todos os tempos: semanas, décadas, atores e diretores mais vistos, **progresso em listas-marco**, gêneros, idiomas e países. "Stats for Lists" mostra elenco e equipe de uma lista. Year in Review para todos | https://letterboxd.com/journal/all-time/ · https://letterboxd.com/journal/2024-year-in-review-faq/ · https://letterboxd.com/journal/hit-list-new-stats-for-lists-feature/ · https://letterboxd.com/year-in-review/ |
| Diário, nota, curtida, revisão | Diário com data, meia estrela, coração (like), "rewatch", tags e review com spoiler. É o modelo que o dono citou | (conhecimento geral; os campos batem com o CSV acima) |
| Link para o filme | `letterboxd.com/tmdb/<id>` redireciona para a página do filme (**não verificado nesta sessão**; conferir antes de usar no QR) | — |

**Implicação para o plano:** um vínculo com o Letterboxd que respeite o ToS tem três peças: (a) **exportar** o nosso diário em CSV no formato de importação deles, gerado no celular; (b) **importar** o ZIP que a própria pessoa baixou, que é o dado dela, enviado por ela, sem robô; (c) **colar o link** de uma review, que guardamos sem ler. Pedir a API oficial é tentativa sem prazo.

## R2. Trakt

| Tema | O que achei | Fonte |
|---|---|---|
| Comentários / reviews | Comentário em filme, série, temporada, episódio ou lista. Mínimo de **5 palavras**. Com **200 palavras ou mais vira "review"** sozinho. Campo `spoiler` e `[spoiler]...[/spoiler]` inline, markdown, curtidas e respostas. `POST /comments` com OAuth | https://trakt.docs.apiary.io/ · https://forums.trakt.tv/t/what-are-the-rules-for-posting-comments/22155 · https://forums.trakt.tv/t/no-spoilers-how-do-i-hide-spoiler-comments/22156 |
| Reações em comentários | O Trakt novo tem 5 ou 6 reações (polegar para cima/baixo, risada, spoiler…) e uma discussão sobre como elas aparecem no endpoint de likes | https://forums.trakt.tv/t/clarification-on-likes-endpoint-behavior-regarding-new-comment-reactions/111196 · https://github.com/trakt/trakt-time/pull/25 |
| Notas | `/sync/ratings`, de 1 a 10. O app já manda 8/5/2 pela reação dos créditos (`src/reacao.h`) | código: `src/reacao.h`, `src/trakt.c` |
| Year in Review | VIP: ano e "todos os tempos", com primeiro e último play, horas, dia da semana, top, gêneros, países, redes, **progresso em listas populares**, mais bem avaliados. O VIP também dá badges | https://medium.com/trakt-tv-blog/all-time-year-in-review-f6f931e4461d · https://medium.com/trakt-tv-blog/year-in-review-5c6ac98f0d3c · https://forums.trakt.tv/t/vip-badges/6554 |
| Limites da conta grátis | Em 01/2025: 2 listas de 100 itens. Revisto depois (2025/2026, **não verificado** na fonte primária): watchlist de 250, 5 listas de 250 cada. VIP: milhares. Por isso **não usamos listas do Trakt para guardar conquistas ou caçadas** | https://github.com/SchizoDuckie/DuckieTV/issues/1447 · https://forums.trakt.tv/t/updating-trakt-limits-for-2026/101592 · https://alternativeto.net/news/2025/2/trakt-tv-has-set-stricter-limits-for-free-users-and-raised-vip-subscription-prices-by-100-/ |
| O que o app já usa | `/sync/history` (100 por página), `/sync/watched/movies` paginado, `/sync/watched/shows`, `/shows/<id>/progress/watched` (`src/trakt.c`, `src/vistoep.h`) | código |

## R3. Serializd e TV Time (séries com amigos)

- **Serializd**, "o Letterboxd das séries": registra episódio, temporada e série, com nota, review, "rewatch", diário em linha do tempo, listas e seguir amigos.
  - https://www.serializd.com/ · https://www.tvscholar.com/p/is-serializd-the-tv-fans-letterboxd · https://www.achriom.com/blog/serializd-vs-trakt/ · https://www.achriom.com/blog/serializd-vs-tv-time/
- **TV Time** (25 mi de pessoas): nota por episódio, **emoção e personagem favorito por episódio**, memes e, o ponto que nos interessa, a **regra "sem spoiler"**: comentários e reações do episódio só aparecem depois de marcá-lo como visto. Amigos "spoiler-free". **Badges** por maratona (vários episódios no mesmo dia), por dias seguidos e por desafios.
  - https://apps.apple.com/us/app/tv-time-show-movie-tracker/id6792364354 · https://en.wikipedia.org/wiki/TV_Time · https://docs.simkl.org/how-to-use-simkl/faq/frequently-asked-questions/simkl-alternatives/simkl-vs-tv-time · https://www.webfactoryltd.com/top-6-tv-series-tracker-apps-people-use-to-avoid-spoilers-while-staying-up-to-date/

**Implicação:** a reação por episódio (emoção) e o "só mostra depois de ver" são padrões consagrados. O nosso borrão de spoiler da trava segue a mesma regra.

## R4. Plex, Netflix e Apple TV (perfis e social)

- **Plex "Discover Together"** (2024/25): notas, reviews, watchlist e histórico compartilhados, feed de atividade e perfis públicos. **Privacidade em níveis**: público, quem tem conta Plex, amigos de amigos, amigos ou privado. Igual ao nosso `alcance`, o que valida o modelo. Em 10/2026 o app de Android TV do Plex ganhou nota, review curta e feed na TV.
  - https://www.engadget.com/entertainment/streaming/plex-update-adds-public-reviews-and-profiles-140050631.html · https://support.plex.tv/articles/user-reviews/ · https://support.plex.tv/articles/friends/ · https://support.plex.tv/articles/follow/ · https://www.prnewswire.com/news-releases/plex-brings-first-ever-integrated-community-features-to-global-streaming-301973379.html · https://www.androidheadlines.com/2026/10/plex-social-discovery-features-android-tv-update.html · https://www.howtogeek.com/plex-launches-user-reviews/
- **Netflix "Moments"** (10/2024): salva uma cena num marcador pessoal e a compartilha. O "My Netflix" é a aba pessoal. Serve de referência para o "Voltar para onde dormiu" e para marcadores.
  - https://techcrunch.com/2024/10/28/netflix-moments-feature-lets-you-save-and-share-clips-from-your-favorite-series/ · https://deadline.com/2024/10/netflix-launches-new-feature-moments-share-send-scenes-1236159757/
- **Apple TV**: um perfil por Apple ID, cada um com o próprio "Up Next". SharePlay para assistir junto pelo FaceTime. Não há trava entre perfis.
  - https://support.apple.com/guide/tv/use-family-sharing-atvb26e8f089/tvos · https://support.apple.com/en-am/guide/tv/atvb0753b37d/17.0/tvos/17.0 · https://developer.apple.com/videos/play/wwdc2022/110384/

## R5. Gamificação: conquistas que parecem merecidas

- **Efeito de sobrejustificação**: recompensa externa por algo que a pessoa já gosta de fazer pode tirar a graça. Badge por tarefa trivial (entrar no app, baixar arquivo) desvaloriza todos os outros.
  - https://yukaichou.com/behavioral-analysis/overjustification-effect-lepper-greene-intrinsic-motivation/ · https://www.growthengineering.co.uk/dark-side-of-gamification/
- **O que funciona**: badges e gráficos de desempenho aumentam a sensação de **competência** e de **tarefa significativa**. Histórias, avatares e companheiros aumentam a de **pertencimento**. Badge vale quando representa esforço ou habilidade real.
  - https://www.sciencedirect.com/science/article/pii/S074756321630855X · https://imotions.com/blog/insights/research-insights/gamification-that-works/ · https://arxiv.org/pdf/2008.06125 (badges no Stack Overflow: "one size does not fit all")
  - https://arxiv.org/pdf/2203.16175 (mau uso de gamificação num app de idiomas: perseguir streak no lugar de aprender)
  - https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8037535/ (meta-análise)
- **Raridade (Steam)**: cada conquista mostra a % de donos do jogo que a têm, e a rara vira motivo de orgulho. No nosso caso, "raridade entre amigos" é mais honesta e mais divertida que uma % global com base pequena.
  - https://partner.steamgames.com/doc/features/achievements · https://www.steam-achievements.com/

**Regras que tirei daqui para o plano (B.1):** sem badge de presença; progresso visível ("7 de 12") em vez de surpresa; metas que a própria pessoa escolhe (caçar uma diretora); streak gentil (semanas, não dias, e com folga); conquistas engraçadas privadas por padrão; nunca interromper o filme.

## R6. Wrapped / retrospectivas

- **Por que funciona**: é pessoal, compartilhável, sem condição. Cartões no formato de story, feitos para print. Autoconhecimento + autoexpressão + comunidade ("distinção ótima": pertencer e se destacar ao mesmo tempo). Narrativa em vez de painel. Em 2025, "corrida" dos top artistas e o "Wrapped Party" multiplayer com amigos.
  - https://adma.com.au/resources/personal-shareable-no-strings-attached-the-winning-formula-behind-spotify-wrapped · https://uxplaybook.org/articles/spotify-wrapped-ux-design-lessons · https://www.soundguys.com/spotify-wrapped-2025-149778/ · https://newsroom.spotify.com/2025-12-03/wrapped-for-artists-songwriters-creators-authors-advertisers/ · https://blogs.lse.ac.uk/impactofsocialsciences/2025/01/30/how-algorithmic-events-like-spotify-wrapped-can-reveal-what-tech-companies-really-know-about-us/

**Implicação:** a TV conta a história em slides, e o **celular** é onde se compartilha (cartão 9:16 gerado no próprio navegador, Web Share). A versão "com amigos" (comparar a retrospectiva) aproveita o duelo do Perfil 2.0, que já existe.

## R7. Dados para trilhas e filmografias (licença)

- **TMDB**: `/person/{id}/movie_credits` (crew com `job: "Director"`), `/collection/{id}` (sagas) e `/movie/{id}/external_ids`. O app já usa TMDB na TV (`src/pessoa.h` com `combined_credits`, `src/diretor.h`). O Worker **ainda não tem chave TMDB** (só a lista de hosts de imagem em `social.js`). Termos de cache: conferir (https://www.themoviedb.org/api-terms-of-use).
- **Oscar e prêmios**: Wikidata (dado CC0) por SPARQL, com `award received` (P166) e `for work` (P1686), mais o imdb (P345) e o tmdb (P4947).
  - https://www.wikidata.org/wiki/Q19020 · https://github.com/gbonk/wikidata-oscar-nominated-films · https://www.wikidata.org/wiki/Wikidata:SPARQL_query_service/queries/examples
- **IMDb Top 250**: os datasets do IMDb são **só para uso pessoal e não comercial**, não trazem o ranking Top 250 pronto, e o IMDb proíbe scraping. **Não usar "IMDb Top 250" com esse nome.** Alternativas: os mais bem avaliados do TMDB, ou uma lista pública do Trakt (a API de listas públicas só precisa do client id do app, como `listas.h` já faz).
  - https://en.wikipedia.org/wiki/IMDb · https://scrapfly.io/blog/posts/how-to-scrape-imdb (só pela citação dos termos)

## R8. Outros apps de "assistir com amigos" consultados de passagem

- Couch Crew, Sofa Time, WatchBuddy, Matched, WatchPick (escolher filme a dois "estilo Tinder"). Servem de ideia para o "Encontrar nossa sessão", que já está no protótipo universo-2026, e para a "nota relâmpago" no celular.
  - https://play.google.com/store/apps/details?id=com.couchcrew.app · https://sofatime.app/ · https://watchbuddy.tv/ · https://apps.apple.com/us/app/-/id1623287922 · https://apps.apple.com/app/id6759346000

## R9. O que li no código (somente leitura, `/Volumes/ExternalSSD/nv-202-amigosdup`, HEAD 18847c70)

| Peça | Onde | Por que importa |
|---|---|---|
| Perfil e Stats 2.0 | `src/perfil.c/.h` (tela única, 3 paradas de foco, duelo com amigo) | **Só funciona com Trakt** (`trakt_perfil`, `app.c:carregarPerfil`). Sem Trakt a tela diz "Trakt desconectado". O Diário próprio resolve isso |
| "Jornada" | Só no protótipo `design/universo-2026/app.js` ("Sua jornada nas histórias": em andamento / para outro momento / concluídas, grade de episódios, "Rafa está no mesmo ponto") | **Não existe no app.** É desenho. A trava de série realiza a parte "Rafa está no mesmo ponto" |
| Conquistas | Nenhum módulo (grep por conquista/achievement/troféu/medalha) | Tudo novo |
| Episódios vistos | `src/vistoep.c/.h` (mapa por título do perfil ATIVO; fontes: conta Nuvio e Trakt) | Base do "quem está onde" na trava |
| Progresso | `src/progresso.c/.h` (`progresso.txt` com coluna de perfil, todas os perfis da TV) | A TV já sabe do progresso local de cada perfil (`prog_continuar_de_perfil`) |
| Conta Nuvio | `src/perfis.h` (`temPin` de `sync_pull_profile_locks`, `perfis_verificar_pin` **bloqueia**), `src/contapend.c` (`sync_push_watched_items` / `sync_delete_watched_items` com `p_profile_id`), `src/syncprog.c` (`sync_push_watch_progress`) | O modo Juntos pode marcar visto nos outros perfis pela conta |
| Borrão de spoiler | `AJ_DET_BLUR_NAO_VISTOS`, `AJ_CW_BLUR_PROX` (`src/ajustes.c`), `gfx_desfocado`, `GFX_BLUR` | Reaproveitar no "à frente da trava" |
| Ilha | `src/ilhaacao.h` (ação feita + Desfazer), `src/ilhasinais.c`, `src/avisos.h`, `src/enquete.h` | Avisos de trava, conquista e retrospectiva |
| Celular | `src/celular.h` (servidor HTTP na LAN, token de uso único; LG e Samsung **não medidos**), `src/celbotao.h`, `src/qr.h` (versão até 6 = 134 bytes) | Pode servir de alternativa sem nuvem. O plano prefere a página na nuvem do Watch Together |
| Reação nos créditos | `src/reacao.h` (Gostei/Mais ou menos/Não gostei → Trakt 8/5/2, por perfil) | Vira o atalho de nota do Diário |
| Amigos no título | `src/amigostitulo.h` (quem viu, gostou, nota, resposta, % de gosto) | Ganha a review |
| Cor da arte | `src/corviva.h` (cor dominante tirada dos pixels que o decode já tem) | Dado do "Ano em cores" de graça |
| Pessoa/diretor | `src/pessoa.h` (TMDB `combined_credits`), `src/diretor.h` | Porta de entrada da Caça |
| Fileira na Home | `src/colfileiras.h`, `src/colecoes.h` (fontes TMDB `PERSON|DIRECTOR|COLLECTION`) | O "Fazer fileira" da Caça |
| Servidor | `servidor/recomendacoes` (D1 `nuvio-recomendacoes`, migrações 001 a 009; a 010 é do Watch Together) | Migrações 011 e 012 deste plano |
| Watch Together | `/Volumes/ExternalSSD/nv-watch-together/docs/plans/watch-together/` (Worker `nuvio-juntos`, DO `Sala`, bilhete HMAC, página do celular `/s/<código>#<bilhete>`; só o marco 1 feito, nada publicado) | O celular deste plano reaproveita o bilhete e a casca da página |
