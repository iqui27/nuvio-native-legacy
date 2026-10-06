# API do Diário (`src/diario.h`)

Branch `feat/sp-diario`. Contrato para a Trava (`feat/sp-trava`) e as Conquistas
(`feat/sp-conquistas`). O header é a fonte da verdade; este arquivo resume e
registra mudanças.

## Modelo

- **Uma entrada = uma vez vista.** Episódio = entrada própria. Nível de título =
  `temporada = episodio = 0` (filme, ou série inteira marcada).
- `id` é o id do **título** sem `:T:E` (`tt123`). Pode passar `tt123:2:5` na
  escrita: é separado em id/T/E.
- `perfil`: índice do perfil (1..8, o de `perfis_ativo()`). **0 = o ativo.**
  Todos os perfis da casa podem ser lidos (a Trava precisa disso).
- Fusão: mesmo (id,T,E) com `quando` < 6 h = mesma vez. Confiança p > t > c > l > m.
- `REVER` é calculado pelo diário. `nota10` 0 = sem nota, 1..10 = meia estrela a 5 estrelas.
- Reação dos créditos → nota: `diario_nota_da_reacao(1|0|-1)` = 8 / 5 / 2.

## Campos de `DiarioEntrada`

`id, tipo ('m'|'s'), origem ('p','t','c','l','m'), temporada, episodio, quando (epoch fim),
seg (tocando), hora (dia*24+hora do início, -1), nota10, comPerfis (bits perfis juntos),
flags, tags (bits DIARIO_TAG_*), tmdb, cor (0xRRGGBB), dormiuSeg, durSeg`.

Flags: `REVER 1, DORMIU 2, CORACAO 4, JUNTOS 8, IMPORTADO 16` (iguais ao servidor),
`DATA_APROX 32, DORMIU_SUS 64, REVIEW 128, PARCIAL 256` (locais; sobem como inteiro).

## Funções que vocês provavelmente usam

| Para | Função |
|---|---|
| Visto? quantas vezes, primeira/última data, nota | `diario_titulo(perfil, id, &DiarioTitulo)` |
| Episódio visto e quando | `diario_episodio(perfil, id, t, e, &quando)` |
| Até onde o perfil chegou na série (Trava) | `diario_ultimo_episodio(perfil, id, &t, &e, &quando)` |
| Tudo de um título | `diario_por_titulo(perfil, id, buf, max)` (devolve o total; buf NULL conta) |
| Janela de datas | `diario_por_periodo(perfil, de, ate, buf, max)` |
| Por pessoa (filmografia → vistos) | `diario_por_lista(perfil, ids, n, saida)` — a lista vem do meta cache de vocês |
| Filtro genérico | `diario_percorrer(perfil, &DiarioFiltro, cb, ctx)` (cb roda com o mutex: não chame o diário dentro) |
| Varredura do índice de títulos | `diario_n_titulos` + `diario_titulo_i` |
| Mudou algo? (qualquer fio) | `diario_geracao(perfil)` |
| Ouvir eventos (laço principal) | `diario_ouvir(fn, ctx)`: `NOVA`, `ALTERADA`, `APAGADA`, `RECARREGADO` (e == NULL) |
| Gravar uma vez vista | `diario_adicionar(perfil, &e)` (1 nova, 0 fundida) |
| Marcar juntos (Trava "Juntos") | `e.comPerfis` + `DIARIO_F_JUNTOS` |

Ordem "em ordem" (Conquistas): `diario_por_lista` dá `primeiro`/`ultimo` por título.

## Fios

Tudo é seguro de qualquer fio. Ouvintes só no `diario_passo()` (laço principal).

## Servidor

Migração `servidor/recomendacoes/migracao-011-diario.sql`: **só** as tabelas do
diário (`diario`, `diario_cofre`, `diario_lb_pendente`). O `filme_meta` e o
`filmografia` do plano 10.1 ficam com as Conquistas (sugestão: `migracao-011-meta.sql`,
para não colidir). Rotas: `POST /v1/diario/lote`, `GET /v1/diario?desde=`,
`DELETE /v1/diario`, `GET /v1/diario/letterboxd.csv`, `POST /v1/diario/letterboxd`,
`POST /v1/diario/link`.

## Mudanças na API

- 06/10: versão inicial (header + stub).
- 06/10: implementação real em `src/diario.c` (mesma API). Teste: `tests/diario.sh`.
