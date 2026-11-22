# tests/farol_cenas — corpus compartilhado do FarolCena v1

Corpus de validacao do parser/validador `src/farolpeca.c` (lote sense,
issue 39; formato em `lotes/sense/PLANO-GOSTO.md` secoes 4.1 e 4.2).
A pasta e ESTAVEL: a TV confere aqui e o servidor porta o mesmo
validador e confere os mesmos arquivos antes de aceitar uma cena do
LLM. Mudou o schema, muda a pasta e a versao.

## Formato

- `boa-NN.json` — 40 cenas validas. Cobrem as sete pecas (fileira, dica,
  porque, comparar, escolha, info, achados), todos os enums (forma, eixo,
  acao.tipo, selo.k, info.k), referencias `c<n>` e `tt<digitos>`, texto por
  modelo `{"m","a"}`, texto livre `{"x"}`, `x` reprovado caindo no `m` de
  reserva, cena sem opcionais, com opcionais zerados e ate o teto de 6 pecas.
- `ruim-NN.json` — 112 cenas invalidas. ~20 recusam a cena inteira
  (`FCE_*`); as demais tem uma ou mais pecas recusadas (`FPE_*`) ao lado de
  pecas boas — a boa continua valendo. Cobrem chave desconhecida, tipo JSON
  errado, enum fora da lista, `c<n>` fora da lista de candidatos, item
  duplicado, `x` reprovado sem reserva, faixas numericas, a 7a peca e a
  cena acima de 8 KB.
- `casos.json` — manifesto que amarra os dois lados:

```json
{"v":1, "nota":"...",
 "casos":[
   {"arq":"boa-01.json","cand":12,"rc":"OK","boas":1,"rec":0},
   {"arq":"ruim-64.json","cand":12,"rc":"OK","boas":1,"rec":1,
    "erros":{"1":"FPE_REF"}},
   {"arq":"ruim-01.json","cand":12,"rc":"FCE_TAMANHO"}]}
```

- `arq`: arquivo relativo a esta pasta, uma cena por arquivo.
- `cand`: `nCandidatos` — quantos candidatos `c<n>` a cena pode referenciar
  (a lista em si nao viaja no JSON; quem chama informa o tamanho).
- `rc`: `"OK"` = `farolpeca_ler` devolve >= 0; `"FCE_*"` = devolve `-FCE_*`
  e a cena inteira foi recusada (mapeado por `farolpeca_cena_erro_nome`).
- `boas`/`rec`: `nPecas`/`nRecusadas` esperados.
- `erros`: mapa posicao-no-array-`pecas` -> nome `FPE_*` esperado em
  `FarolCena.erro[pos]` (posicoes omitidas esperam `FPE_OK`); posicoes alem
  da 6a nao tem `erro[]`, so contam em `rec` (mapeado por
  `farolpeca_erro_nome`).

## Referencia na ponta a ponta

```json
{"v":1,"tela":"home","perfil":2,"feita":1795000000,"vale_s":86400,
 "origem":"regras","pecas":[
  {"t":"fileira","id":"gosto1",
   "titulo":{"m":"FT_FIL_PORQUE_TERMINOU","a":["tt11280740"]},
   "forma":"cartaz","itens":["c0","c4","c1","c7","c2","c9"],
   "motivo":{"c0":{"m":"FT_MOT_PESSOA","a":["p137427"]}}}]}
```

Obrigatorios do envelope: `v` (=1), `tela`, `pecas`. `perfil`, `feita`,
`vale_s` e `origem` sao opcionais. Chave desconhecida no envelope recusa a
cena; dentro de uma peca, recusa so ela. `itens`, `alvo`, `escolha`, chaves
de `motivo` e argumentos `c<n>` resolvem contra `cand` titulos.

`tests/farol_peca.c` roda o corpus inteiro; `tests/farol_peca.sh` compila e
executa sob ASan/UBSan.
