# Social: contrato TV <-> servidor (redesenho de 02/10/2026)

Servidor: `servidor/recomendacoes/src/social.js` (+ `index.js`). Tabelas:
`migracao-006-social.sql`. Cliente: `src/recomenda.h` (secao "REDESENHO").
Testes: `servidor/recomendacoes/teste-social.sh`, `tests/recomenda_social.sh`.

## Identidade

- Cabecalhos de sempre: `Authorization: Bearer <token>` + `X-Nuvio-Auth: nuvio|trakt`.
- **Novo**: `X-Nuvio-Perfil: <profile_index>` so quando o perfil ativo NAO e o
  principal. Id da pessoa: `nuvio:<sub>:<indice>`. Principal (sem cabecalho)
  continua `nuvio:<sub>` — nenhum dado antigo muda de dono. Trakt ignora.
  Valor fora de 1..32 ou nao numerico = principal.
- `POST /v1/eu` aceita `{"nome": "<nome do perfil>", "avatar": "<url>"}`
  (corpo vazio continua valido). Avatar so `https` de host conhecido (host do
  `SUPABASE_URL`, Trakt, ou a lista `AVATAR_HOSTS` do worker); senao "".
  Resposta ganha `nome` (pronto), `exibicao`, `alcance`, `perfil`.
- Nome pronto (`pessoa.nome`): exibicao > nome do perfil (Nuvio) / da conta
  (Trakt) > `Amigo #<rowid>`. Nunca o id.
- `POST /v1/eu/nome {"nome": ".."}` — nome de exibicao (letras, numeros,
  espaco, `'` e `-`, ate 32). `""` apaga e volta ao nome do perfil.

## Privacidade (alcance)

`POST /v1/alcance {"nivel": 0|1|2}`. `pessoa.alcance`: -1 nao respondeu (=0),
0 ninguem, 1 so amigos, 2 amigos de amigos. Conferido em toda escrita e
leitura. Nivel 0 apaga eventos, "agora" e agregados.

Decisao: **separado** do `descobrivel` ("quem pode me achar") e do antigo
`perfil.ativ` (rota velha `/v1/amigos/atividade`). Motivo: perguntas
diferentes; e `ativ` antigo nao vira alcance (eventos novos — abandono,
reacao, tempo — sao mais do que aquela resposta aceitou).

## POST /v1/atividade (eventos do player)

Exatamente o contrato pedido; **nada mudou**:

```
{"ev":"inicio"|"progresso"|"fim"|"abandono"|"reacao"|"salvo", "imdb":"tt..",
 "midia":"movie"|"series", "titulo":"..", "poster":"https://..", "temporada":0,
 "episodio":0, "pct":0-100, "seg":<s neste trecho>, "reacao":1|0|-1, "rec":<id|0>}
```

Resposta: `{"ok":1,"guardado":0|1,"evento":0|1}` (`guardado:0` = alcance < 1).
Detalhes do servidor:
- `progresso`: so atualiza "assistindo agora" (vence 15 min) e agregados; nao
  vira evento. `inicio` tambem liga "agora"; `fim`/`abandono` desligam.
- **06/10/2026 — sair do player nao e mais evento.** O cliente novo manda
  `progresso` com `"parou":1` (fecha o "agora" em vez de renova-lo); o
  `abandono` de cliente antigo faz o mesmo e tambem NAO vira linha de `evento`.
- Mesmo ev+titulo+episodio+reacao em 10 min = um evento so.
- `seg` limitado a 4 h por evento; `poster` so de host conhecido (tmdb,
  metahub, amazon, tvdb, trakt), senao vazio.
- `rec`: so altera a rec se ela foi mandada PARA quem envia
  (`inicio`/`progresso` -> comecou, `fim` -> terminou, `reacao` -> reacao).
- Limite 600 eventos/h por pessoa. Retencao 90 dias.
- **Corpo antigo** (sem `ev`: `imdb,tipo,titulo,ano,nota,agora`) continua
  aceito na mesma rota e vai para a logica antiga de `amigos.js` — TVs no ar
  e o `enviarAtividade` atual de recomenda.c (fileira "Entre amigos" via
  `perfil.ativ`) usam. A rota decide pelo campo `ev`.

## POST /v1/rec/resposta (resposta direta a uma rec recebida)

Migracao: `migracao-007-resposta.sql` (colunas `rec.resposta`, `rec.respondido`).
**Rodar a migracao ANTES do deploy** (o `GET /v1/amigo` passa a ler as colunas).

```
{"id": <rec>, "reacao": 1|0|-1|null, "texto": "a-z0-9 e espaco, ate 60"}
```

Resposta `{"ok":1,"n":0|1}`. Marca `comecou=terminou=visto=aberto=1` e grava
`respondido` (epoch). `reacao` null/ausente e texto vazio NAO apagam o que ja
havia. So a rec mandada PARA quem responde muda (`n:0` para id alheio). NAO
passa pelo alcance: e um gesto explicito ("Fulano vai ver sua resposta"), nao
atividade automatica. Limite 120/h por pessoa. `GET /v1/amigo` ganha
`resposta` e `respondido` em cada item de `recs`.

Cliente: `src/recresp.c` (estado local em `recomendacoes-respostas.txt`,
enviado pelo fio de `recomenda.c`). Fontes do gesto: "Ja assisti" no menu do
OK longo da aba Amigos, o fim no player (`atividade.c`, mesmo "fim" do resto do
app) e o cartao "O que achou?" dos creditos (`reacao.c`, passo 2: mensagem).

### GET /v1/rec: o estado de "Assistidas" nos outros aparelhos (W22)

Sem migracao nova: le `rec.terminou`, `rec.reacao`, `rec.resposta`,
`rec.respondido` (migracoes 006 e 007). Resposta (campos novos em **negrito**;
cliente antigo ignora, cliente novo tolera a ausencia):

```
{"cursor": N, "novas": N,
 "itens": [{... campos de sempre ..., "terminou":0|1, "reacao":1|0|-1|null,
            "resposta":"", "respondido":0}],
 "respostas": [{"id":<rec>, "terminou":0|1, "reacao":1|0|-1|null,
                "resposta":"", "respondido":<epoch>|0}]}
```

- `respostas` traz o estado de TODA rec da pessoa que ja foi assistida
  (`terminou=1`), respondida (`respondido>0`) ou reagida, ids velhos inclusive —
  `?desde=` so entrega ids novos, e uma rec respondida em outra TV tem id velho.
  Ate 120, mais nova primeiro, sem cartaz/titulo (a TV ja tem a rec).
- O ETag passou a `"<n>-<maiorId>-<naoVistas>-<respostas>.<sig>"`: responder em
  outra TV invalida o 304. A primeira sondagem depois do deploy e um 200.
- Cliente (`recomenda.c` -> `recresp_do_servidor`): funde no estado local. Linha
  com mudanca local ainda nao enviada (`versao > enviada`) nao e tocada; fora
  isso o servidor so acrescenta (assistida/respondida nunca voltam atras) e a
  reacao/mensagem dele vence se `respondido` for mais novo que a ultima mudanca
  local ou se aqui nao houver valor. O que veio do servidor nao e reenviado.
- `GET /v1/amigo` (quem MANDOU ve a resposta): `recs[].resposta` e
  `recs[].respondido` ja existiam; o perfil do amigo agora os mostra (cartaz
  "viu · gostou / mais ou menos / nao gostou" e, em foco, a mensagem).

## GET /v1/feed?desde=<id>

So o que diz algo (06/10/2026): `fim`, `reacao` e o `inicio` da PRIMEIRA vez
de uma serie (filme comecado, episodio seguinte, `abandono` e `salvo` ficam de
fora — as linhas antigas continuam na tabela, so nao saem). UMA LINHA POR
PESSOA E TITULO: os `fim` de uma serie viram uma linha com `"eps":<episodios
distintos>` (so quando > 1) e a reacao mais nova do mesmo titulo vai junto do
`fim` em `"reac":1|0|-1`. De quem eu posso ver, mais novo primeiro, ate 50, com
`ETag`/`If-None-Match` (304). O cliente pede sempre `desde=0` (para quem baixa
o nivel sumir da lista).

```
{"cursor":11,"itens":[{"id":11,"de":"nuvio:aaa"|"pub:<handle>","deNome":"..",
 "deAvatar":"..","grau":1|2,"via":"<amigo em comum, grau 2>","ev":"reacao",
 "imdb":"tt..","midia":"movie","titulo":"..","poster":"..","temporada":0,
 "episodio":0,"pct":0,"reacao":1,"criado":1790974381}]}
```

Grau 2 (amigo de amigo, so com alcance 2): id vira handle opaco, sem foto.
Bloqueio (qualquer sentido) esconde. Limitacao: o ETag e maior id + contagem.

## GET /v1/amigo?id=<id de contato | pub:handle>

```
{"id","nome","avatar","grau","via","desde","origem":"codigo|trakt|sugestao|pedido|",
 "compartilha":0|1,
 "mes":{"mes":"2026-10","seg":2700,"filmes":1,"series":1}|null,
 "agora":{imdb,midia,titulo,poster,temporada,episodio,pct,atualizado}|null,
 "gostou":[{imdb,midia,titulo,poster,criado}] (ate 10),
 "recs":[{id,imdb,tipo,titulo,poster,criado,"estado":"entregue|aberta|comecou|terminou|reacao","reacao":1|0|-1|null}] (so grau 1, ate 20),
 "gosto":{"total","iguais","pct"}|null}
```

404 para quem nao e contato nem amigo de amigo visivel (mesma resposta de
"nao existe"). `gosto` so quando os dois tem alcance >= 1. `origem` vazio =
contato de antes da migracao.

`GET /v1/contatos` ganhou `desde` e `via` em cada contato.

## Merge de fontes (cliente)

`RecEvento` (recomenda.h) e o formato unico. `rec_eventos_unir` deduplica por
pessoa + imdb + acao (reacao tambem pelo valor) com |dt| <= 1 h, ou sempre
que um dos dois nao tem hora; na fusao vence o do nosso servidor, completado
pelo outro. `recomenda_feed_unido` junta o cache do feed com os itens de
`trakt_social`. Trakt "assistindo agora" = `REC_ACAO_INICIO`, "assistiu" =
`REC_ACAO_FIM`, "avaliou" = `REC_ACAO_NOTA`.

Simkl e Letterboxd: nao implementados (ver docs/social-fontes.md). Se o dono
decidir, a UI precisa pedir: Simkl = id NUMERICO do amigo; Letterboxd =
usuario do amigo (idealmente informado pelo proprio amigo); e o Letterboxd so
traz TMDB id (precisa TMDB->IMDb antes do dedupe).

## Ligacao com o modulo do player (agente/reacao, src/atividade.c)

- Permissao: no arranque, `recomenda_ao_mudar_alcance(atividade_definir_permitido)`.
  Chama na hora e a cada mudanca de nivel, fora do mutex; nao perguntado = 0.
- Identidade: `atividade.c` monta os cabecalhos sozinho e **nao manda
  `X-Nuvio-Perfil`** — num perfil nao principal os eventos cairiam na pessoa
  do principal. No merge, trocar o `identidade()` dele por
  `recomenda_cabecalhos(cab /*[4]*/, aut, .., via, .., perfil, ..)`.
- `recomenda_atividade()` (fila propria em recomenda.c) faz o mesmo papel;
  com o atividade.c no merge, um dos dois sobra — sugestao: ficar com o
  atividade.c (tem fila em disco) e apagar `recomenda_atividade`.
