# STATUS — Caça à filmografia + conquistas (Onda 2, branch `feat/sp-conquistas`)

Atualizado em 06/10/2026. Nada foi publicado: sem push, sem deploy, sem migração aplicada, sem instalar em TV.

## Feito

| Peça | Onde | O que faz |
|---|---|---|
| Motor puro | `src/conquista.c/.h` | Degraus (Primeiro passo, Metade, Quase lá, Completo, Em ordem), regra "Em ordem" (precisa da data da 1ª vez de todos; rever não conta), desbloqueio que nunca perde, orçamento de **1 aviso por sessão** (o resto vira "+N"), privacidade (`alcance ≥ 1` e "Mostrar aos amigos"; engraçadas e perfil infantil nunca) |
| Adaptador do "visto" | `src/cacavisto.c/.h` | Hoje: histórico do catálogo (Trakt + conta Nuvio + gesto local) e progresso ≥ 90% (única fonte com data). **Trocar pelo Diário = mexer só neste arquivo** |
| Caçadas | `src/caca.c/.h`, `src/cacadados.c` | Por perfil (`cacas-p<N>.txt`): diretor, ator, saga TMDB, top 250 TMDB, lista pública do Trakt, trilha do servidor. Um fio de rede: Worker `/v1/filmografia` primeiro, senão TMDB direto (chave que a página da pessoa já usa) + fila lenta de imdb/duração em `caca-meta.txt`. Longas por padrão; "+ Documentários" e "+ Curtas" opcionais; filme de TV nunca. Semeia sem festa ao criar (nada sai por abrir o app). Recalcula com debounce de 2 s |
| Aviso | `caca.c` (`avisar`) | `ilhaacao_feita`: "Conquista: Completo · Denis Villeneuve", voo da foto, modal "Você viu todos os 10 · 1998–2021". Só com o player fechado (`caca_passo(agora, !player_aberto())`) |
| Tela Caça | `src/cacaui.c/.h` (`TELA_CACA`) | Sugestões ("faltam N"), + Caçar novo (vai à Busca), + Nova trilha (seletor), lista com x/y e barra, detalhe "Você viu 2 de 11 · faltam 9 · anos", pôsteres (falta = apagado, visto = selo), legenda dos degraus, Fazer fileira / Fazer coleção / + Documentários / + Curtas / Amigos veem·Só eu vejo / Parar. Atribuição TMDB no pé |
| Entradas | `perfil.c`, `detail.c` | Botão "Caça" no Perfil (CIMA de qualquer parada; funciona sem Trakt). Página da pessoa: "Você viu X de Y" + "Caçar" (CIMA na 1ª linha da filmografia) |
| Home | `colecoes.c` (`col_extra_definir_grupo`), `descoberta.c` (fonte `caca`) | "Fazer fileira" = pasta no grupo "Caçadas" que abre só o que falta e some um dia depois de completa; "Fazer coleção" = grupo "Coleções da caça" com todos. Pastas extras agora são por dono (listas fixadas e Caça não se apagam) |
| i18n | `idioma_tab.h` + 28 irmãs | 46 chaves; `tools/idiomas.py` ok. O degrau é "Completo" porque "Completa" já é chave (legendas, "Thorough") |
| Ícones | `aj_trophy/crosshair/circle/contrast/hexagon/ribbon` | Lucide 1.48.0, linha nova em `tools/icones-lucide.sh` |
| Servidor (não publicado) | `servidor/recomendacoes/src/conquista.js`, `trilhas-dados.js`, `migracao-013-conquista.sql` | `GET /v1/filmografia` (cache D1 14 dias, compartilhado, segredo `TMDB_KEY`), `GET /v1/trilhas`, `GET /v1/trilha?id=`, `POST /v1/conquista` (nível só sobe), `/v1/amigo` traz até 6 conquistas visíveis (alcance conferido na leitura) |
| Trilhas de prêmio | `tools/trilhas-wikidata.py` | Wikidata (CC0): Oscar de Melhor Filme (98) e Filme Internacional (71) |

## Testes

- `tests/conquista.sh` (motor), `tests/caca.sh` (classe, escopo, leitores TMDB/Worker, progresso) — ok, com ASan.
- `servidor/recomendacoes/teste-conquista.mjs` — ok.
- Build Mac sem warnings novos; `-DNV_TPK -fsyntax-only` nos arquivos mexidos ok.
- Capturas: `NUVIO_SHOT_FONTE=3 bash tests/caca_shot.sh` (vazio, Verbinski 2/11, pôster em foco, botões, Villeneuve, seletor de trilhas, aviso na ilha e modal).

## Falta / depende do dono

1. **Deploy**: `wrangler secret put TMDB_KEY`, aplicar `migracao-013-conquista.sql`, publicar o Worker. Sem isso a TV usa o TMDB direto e as conquistas não chegam aos amigos (tudo local funciona).
2. **P5**: escolher a lista pública do Trakt dos "250 mais bem avaliados" (`TRILHA_TRAKT_TOP250` em `[vars]`). Hoje só o top 250 do TMDB aparece.
3. **Diário**: quando `feat/sp-diario` entrar, trocar `cacavisto.c` pelo `diario.h` (primeira vez + data), e "Em ordem" passa a sair de verdade (hoje só com datas do progresso local).
4. Não feito nesta rodada: selo procedural com a foto no centro (usei os 5 ícones como formas), modal próprio com [Mostrar aos amigos]/[Ver caçada] (usa o modal genérico do `ilhaacao`, com "FEITO"), raridade entre amigos, conquistas engraçadas/hábito/exploração, chip "Caçada X 8/10" na página do título, aviso pós-créditos "faltam 2", Busca com destino "caçar", mostrar conquistas no perfil do amigo (o servidor já manda `conquistas`).
5. Nada provado em TV.
