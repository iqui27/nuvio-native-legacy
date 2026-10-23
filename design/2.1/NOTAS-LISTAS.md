# Nuvio 2.1: Listas e Trilhas (Biblioteca › Listas)

Mockups estáticos, mesmo quadro de TV do resto de `design/2.1`. Abra `listas.html` e `trilhas.html` (ou pelo `index.html`; a aba Listas da `biblioteca.html` leva para lá). A tecla **D**, ou o botão "dados" da barra, contorna cada elemento pela origem do dado:

- **verde · hoje**: dá para fazer só no app, com o que Trakt, TMDB e o nosso servidor já entregam;
- **âmbar · nosso**: precisa de trabalho no `servidor/recomendacoes` (tabela, rota, script ou texto de curadoria);
- **vermelho · bloqueado**: a fonte não oferece isso a apps, ou só raspando, e raspagem está fora.

Curadores, @handles, diretores, filmes e textos são inventados. "Nova onda japonesa" é um recorte real, mas os 8 filmes e "Kenji Ōmura" não existem. O QR não codifica nada.

## O pedido e onde está

| Pedido do dono | Quadro |
|---|---|
| Explorar listas públicas (Trakt, Simkl, Letterboxd, nossa curadoria), busca, em alta, populares, por tema, diretor, década, país, movimento | 6A |
| Página da lista: descrição, quem fez, por quê, quando, "história da lista", nota do curador por filme, seguir, salvar, importar, progresso | 6B |
| Listas "com os usuários falando" | 6C (comentários, curador marcado, spoiler fechado, comentar pelo celular) |
| Trilhas: jornada em ordem, o porquê de cada passo, continuar de onde parou | 7A |
| Por filme: "Em listas", o que os curadores disseram, "se gostou disso, siga esta trilha" | 7C |
| Páginas de diretor e tema: filmografia como trilha, temas, ordem recomendada | 7B (tema reaproveita o layout) |
| Minhas listas: descrição, nota por filme, compartilhar por QR | 6D |

Regras de TV mantidas: tudo anda no D-pad; texto (busca, comentário, descrição, nota) só pelo celular, com o teclado por QR que já existe (`src/celbotao.h`); fonte mínima de 15 px no quadro de 1920 (18 px ou mais no que se lê de verdade).

## O que já existe no código

- `src/listas.c` / `src/listas.h`: Biblioteca › Listas com três fontes. Trakt: listas da pessoa (`/users/me/lists`), busca (`/search/list`) e em alta (`/lists/trending`), as duas últimas só com o client id. Itens por `/lists/<id>/items` (`src/descoberta.c:7984`). Fixar na Biblioteca e virar fileira na Home (`LST_FIX_MAX` 24). Simkl: só os 5 estados. Conta Nuvio: pastas de coleção. Regra: no máximo **um pedido por aba**.
- Fixture `tests/fixtures/trakt_busca_listas.json`: lista traz `item_count`, `likes`, `comment_count`, `description`, `user`.
- Caça (`servidor/recomendacoes/src/conquista.js`): `GET /v1/trilhas` e `/v1/trilha?id=` (prêmios gerados do Wikidata, CC0, por `tools/trilhas-wikidata.py`, e o top 250 do Trakt) e `GET /v1/filmografia?pessoa=&papel=dir|ator` (TMDB, guardada 14 dias no D1 e compartilhada entre TVs).
- Letterboxd (`servidor/recomendacoes/src/letterboxd.js`): lê o export que a própria pessoa baixou, mas **só** `diary`, `ratings` e `reviews`. Listas do export não são lidas.
- QR de até 134 bytes (`src/qr.h`); código curto no estilo de `servidor/recomendacoes/src/codigo.js`.

## O que cada fonte oferece

- **Trakt** (docs em trakt.docs.apiary.io; o que está no código foi marcado acima): `/lists/trending` e `/lists/popular`; `/search/list`; `/lists/<id>` com `created_at`, `updated_at`, curtidas e comentários; `/lists/<id>/items` com `rank`, `listed_at` e `notes` por item; `/lists/<id>/comments/<ordem>` (recentes, antigos, curtidos, com respostas) e `/comments/<id>/replies`, com marca de spoiler; `/lists/<id>/likes`; `/movies/<id>/lists/<tipo>/<ordem>` (listas que contêm o filme) e `/people/<id>/lists`. Escrita com o token da TV: criar lista, editar descrição, adicionar item, curtir lista, comentar (mínimo de 5 palavras). **Não existe** histórico de edições nem registro do que saiu da lista.
  - **SUPOSIÇÃO** até fixture: `notes` vir no `/items` sem parâmetro extra; nota em item exigir VIP para escrever; limites novos de listas e itens em conta grátis (já marcados como suposição em `PLANO-ECOSSISTEMA.md` §2).
- **Simkl**: a API não tem listas nomeadas nem públicas, só os 5 estados da pessoa (`src/listas.h`). Listas públicas do Simkl: **bloqueado** (conferir de novo o blueprint antes de fechar).
- **Letterboxd**: API fechada, só por convite (`PLANO-ECOSSISTEMA.md` §7). Listas públicas: **bloqueado**, sem raspagem. As listas **da própria pessoa** vêm no export (pasta `lists/`, um CSV por lista com nome, descrição, posição e nota por filme; **SUPOSIÇÃO** até abrir um export real) e cabem no mesmo caminho do diário.
- **Wikidata (CC0)**: movimento (P135), país (P495), data (P577) e direção (P57) de cada filme, com id do IMDb e do TMDB. Dá para gerar "por movimento / década / país" com um script irmão do `trilhas-wikidata.py`.
- **TMDB**: filmografia e biografia (já no `/v1/filmografia`), palavras-chave por filme (base dos "temas que voltam").

## Viabilidade por elemento

| Quadro | Elemento | Fonte | Situação | Nota |
|---|---|---|---|---|
| 6A | Em alta, Populares | Trakt `/lists/trending`, `/lists/popular` | hoje | trending já está em `listas.c` |
| 6A | Buscar listas | Trakt `/search/list` + teclado do celular | hoje | |
| 6A | Cartão: curador, filmes, curtidas | Trakt (`user`, `item_count`, `likes`) | hoje | |
| 6A | Cartão: mosaico e "você viu N" | `/lists/<id>/items` × vistos locais | hoje, com custo | 1 pedido por cartão fura a regra de um por aba; mostrar só em listas já abertas ou seguidas |
| 6A | Temas, diretores, décadas, países, movimentos | Wikidata + TMDB + curadoria | nosso | script gera as listas, servidor serve |
| 6A | Fonte "Curadoria Nuvio" | tabela nova no servidor | nosso | |
| 6A | Fonte Simkl | 5 estados da pessoa | hoje | públicas: bloqueado |
| 6A | Fonte Letterboxd | export da pessoa | nosso | públicas: bloqueado |
| 6B | Título, descrição, curador, criada e atualizada | Trakt `/lists/<id>` | hoje | |
| 6B | História: o que entrou e quando | `listed_at` dos itens, agrupado | hoje | derivado no app |
| 6B | História: o que saiu e por quê | não existe no Trakt | bloqueado | nas listas da Curadoria Nuvio: nosso (revisões com nota) |
| 6B | Nota do curador por filme, ordem | `notes` e `rank` dos itens | hoje | conferir `notes` em fixture |
| 6B | Você viu, Continuar | vistos locais e histórico Trakt | hoje | |
| 6B | Seguir | curtir no Trakt + fixar local | hoje | fixar já existe |
| 6B | Fileira na Home | `listas.c` | hoje | |
| 6B | Copiar para minha lista | Trakt, escrita | hoje | limite de conta grátis: suposição |
| 6C | Ler, ordenar, respostas, spoiler | Trakt comments | hoje | |
| 6C | Marca CURADOR | autor do comentário = dono da lista | hoje | |
| 6C | Comentar | celular → TV → Trakt | hoje | mínimo de 5 palavras |
| 6C | Comentários em listas nossas | servidor + moderação | nosso | ver pergunta 2 |
| 7A | Passos, porquê, "repare em", "se já viu" | texto de curadoria | nosso | |
| 7A | Lista do Trakt aberta como trilha | `rank` + `notes` | hoje | modo de exibição de qualquer lista ordenada com notas |
| 7A | Progresso, continuar | vistos locais | hoje | |
| 7A | Revisão, pessoas na trilha, terminaram | contagem agregada | nosso | anônima; ver pergunta 8 |
| 7A | Trilhas de prêmios | `/v1/trilhas` | hoje | mesmo layout, sem o porquê |
| 7B | Filmografia, anos, biografia | `/v1/filmografia` + TMDB | hoje | |
| 7B | Ordem sugerida, por onde começar | curadoria | nosso | sem curadoria cai para ordem de lançamento |
| 7B | Temas que voltam | palavras-chave TMDB agregadas | nosso | cache no Worker, como a filmografia |
| 7B | Em N listas | Trakt `/people/<id>/lists` | hoje | total pelo cabeçalho de paginação |
| 7B | Página de tema | Wikidata + TMDB + curadoria | nosso | |
| 7C | Listas do Trakt com este filme | `/movies/<id>/lists` | hoje | |
| 7C | Posição e nota em cada lista | `/lists/<id>/items` | hoje, com custo | só nas 3 primeiras |
| 7C | "Se gostou disso, siga esta trilha" | índice filme → trilha no servidor | nosso | |
| 6D | Listas Trakt, pastas Nuvio, estados Simkl | `listas.c` | hoje | |
| 6D | Listas do Letterboxd | export `lists/` | nosso | estender `letterboxd.js`; conferir formato |
| 6D | Nova lista, descrição, nota no filme, ordem | Trakt, escrita, texto do celular | hoje | nota: VIP é suposição |
| 6D | Descrição e nota em pasta Nuvio | a conta não tem esses campos | nosso | ou não fazer |
| 6D | QR de lista pública do Trakt | URL do Trakt (cabe em 134 bytes) | hoje | |
| 6D | Código curto e página no celular (listas Nuvio, Letterboxd, privadas) | servidor | nosso | |

**Resumo:** de 39 elementos, 25 dão **hoje** só com o app (2 com custo de pedidos), 13 precisam do **nosso** servidor (quase todos curadoria) e 1 está **bloqueado** (o que saiu de uma lista do Trakt). Fora da tabela, também bloqueadas: listas públicas do Letterboxd e do Simkl.

## Ordem sugerida

1. **Só app, sem servidor:** em alta, populares e busca (6A só com Trakt); página da lista com história por `listed_at` e notas (6B); comentários (6C); "Em listas" com o Trakt (7C); diretor com filmografia e ordem de lançamento (7B, a Caça já tem os dados); minhas listas do Trakt com QR (6D). Qualquer lista do Trakt com ordem e notas já abre como trilha (7A).
2. **Servidor:** uma tabela de listas curadas (itens, porquê de cada passo, revisões com nota), o índice filme → trilha, as listas geradas do Wikidata (movimento, década, país), os temas TMDB, as listas do export do Letterboxd e o código curto de compartilhar.
3. **Não fazer:** raspar o Letterboxd; inventar listas públicas para o Simkl.

## Mudanças fora das telas novas

- `biblioteca.html`: a aba Listas leva para `listas.html`.
- `tv.js`: o QR ilustrativo saiu do `comecar.html` e foi para o `tv.js`, para servir aos dois; tecla D e botão "dados" aparecem só nas páginas com `data-d`.
- `tv.css`: contornos do mapa de dados.
- `index.html`: cartões 6 e 7.

## Perguntas em aberto

1. **Quem escreve a Curadoria Nuvio?** O dono, convidados do Discord com revisão, ou os dois? Em que idioma, pt e en?
2. **Comentários nas listas nossas:** abrir (precisa denúncia e moderação além do bloqueio que já existe em `social.js`) ou deixar só os comentários do Trakt?
3. **"Seguir" curte no Trakt** ou só fixa no app, sem mexer na conta?
4. **MDBList** como fonte de listas públicas, com a chave da pessoa (decisão Q5)? Ele também oferece listas do Letterboxd, que vêm de raspagem do lado dele: aceitável ou fora?
5. **Onde moram as Listas:** em Biblioteca › Listas, como desenhado, ou ganham item próprio no rail?
6. **"Você viu N" nos cartões de Explorar** custa um pedido por cartão. Mostrar só nas listas já abertas ou seguidas?
7. **Séries em trilhas?** O desenho só tem filmes.
8. **"Pessoas nesta trilha / terminaram":** contagem anônima no servidor serve, ou só conta quem tem alcance ≥ 1?
9. **Listas do Letterboxd do export:** só leitura no app, ou também copiar para uma lista do Trakt?
10. **Limites da conta grátis do Trakt:** confirmar números antes de prometer "Copiar para minha lista" e "Nova lista".

## Decisões do dono (12/10/2026)

1. **Curadoria Nuvio:** fontes da internet + listas de autoria nossa, em pt e en. Da internet só entra o que a licença permite: Wikidata (CC0) livre; Wikipedia (CC BY-SA) com atribuição e link; listas de terceiros (críticos, sites) só como referência com link e créditos, nunca o texto copiado. Texto das trilhas e notas é nosso.
2. **Comentários nas nossas listas:** sim. Exige denúncia, moderação, limite por conta e spoiler fechado por padrão.
3. **Seguir:** só fixa no app; não curte no Trakt.
4. **MDBList:** sim, como fonte de listas públicas (inclui as do Letterboxd que o MDBList oferece; a coleta é do lado deles). Chave do usuário pela conta Nuvio (PLANO-ECOSSISTEMA Q5).
5. **Lugar:** Listas fica dentro da Biblioteca.

Abertas: 6–10 acima.
