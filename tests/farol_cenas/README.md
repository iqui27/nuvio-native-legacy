# tests/farol_cenas — corpus compartilhado do FarolCena v1

Corpus de validacao do parser/validador `src/farolpeca.c` (lote sense,
issue 39; formato em `lotes/sense/PLANO-GOSTO.md` secoes 4.1 e 4.2).
A pasta e ESTAVEL: a TV confere aqui e o servidor porta o mesmo
validador e confere os mesmos arquivos antes de aceitar uma cena do
LLM. Mudou o schema, muda a pasta e a versao.

## Formato

- `boa-NN.json` — 85 cenas validas. Cobrem as sete pecas (fileira, dica,
  porque, comparar, escolha, info, achados), todos os enums (forma, eixo,
  acao.tipo, selo.k, info.k), referencias `c<n>` e `tt<digitos>`, texto por
  modelo `{"m","a"}`, texto livre `{"x"}`, `x` reprovado caindo no `m` de
  reserva, `x` aprovado caindo no `m` quando o pool de `xs` estoura (boa-41),
  cena sem opcionais, com opcionais zerados e ate o teto de 6 pecas,
  `\ud800` solto virando espaco (boa-45/46), escapes `\u00XX` com os
  digitos hexadecimais literais nas bordas `a`/`f`/`A`/`F`/`0`/`9`
  (boa-47), argumento de modelo vazio `""` (boa-49), codepoints nas
  bordas da codificacao UTF-8 (`\u0800`, `\ud800\udc00` — boa-50/51),
  inteiros nas bordas (`perfil` e selo `id` em INT32_MAX, `feita` no
  teto do acumulador int64 — boa-52), `id` no teto de 31 chars e nas
  bordas do charset com hifen/maiuscula (boa-52/53), `escolha`
  apontando o primeiro item `c0` (boa-54), argumentos com bytes UTF-8
  crus validos nas seis bordas 2/3/4-byte (boa-55..60), a cena com
  exatamente 8192 bytes (boa-61), os opcionais numericos aceitando
  `-0`/negativo onde o contrato permite (`perfil:-0`, `feita:-5`,
  `vale_s:-0` — boa-62..64) e os argumentos de modelo na forma
  `c<digitos><letras>`/`"c"` que sao texto livre, nao referencia
  (`"c1x"` com 0, 1 ou 12 candidatos, `"c12x"`, `"c"` — boa-65..70), o
  selo `id:-0` como inteiro valido (boa-77/78), a referencia `c<n>` no
  limite do buffer — 18 e 19 caracteres ainda resolvem (boa-87/88) —,
  o argumento de modelo `""` vazio (boa-100), o par de surrogatas e o
  solitario em argumento de modelo (boa-104/105/108), os argumentos
  `c<digito><pontuacao>` com 0 candidatos (boa-109..113) e o `x`
  contendo apenas o par `\udbff\udfff` na borda do high surrogate
  (boa-115).
- `ruim-NN.json` — 403 cenas invalidas. 135 recusam a cena inteira
  (`FCE_*`); as demais tem uma ou mais pecas recusadas (`FPE_*`) ao lado de
  pecas boas — a boa continua valendo. Cobrem chave desconhecida, tipo JSON
  errado, enum fora da lista, `c<n>` fora da lista de candidatos, item
  duplicado, chave duplicada em `motivo` (literal e apelido `c0`/`c00`) e
  em cada sub-objeto (`m`/`a`/`x` de texto, `tipo`/`alvo` de acao, `k`/`id`
  de selo, `id`/`r` de opcao, `k`/`l` de info), campo faltando em acao,
  selo, opcao e info, chave conhecida de outra peca (FPE_CHAVE), charset
  de `id`/`origem`, `x` reprovado sem reserva, `x` aprovado sem lugar no
  pool e sem `m` (FPE_LIMITE), `a` sem `m`, argumento com controle cru,
  tipo JSON errado em cada posicao, `\u0000` em string, documento
  malformado, faixas numericas, tetos de lista, a 7a peca e a cena acima
  de 8 KB, estrutura torta dentro de cada recipiente (`a`, `motivo`,
  `acao`, `linhas`, `selos`, `eixos`, `opcoes`, `itens`, envelope e o
  objeto da peca), virgula faltando entre itens de array e entre membros
  de objeto, escape simples de controle (`\t` `\r` `\b` `\f` e `\n` sem
  conteudo), par de surrogatas `\ud83d\udcXX` produzindo emoji (FPE_X),
  `v` fora de 1, numero de 64 bits estourando `passo`/`de`/`confianca`/
  `id`/`perfil`/`feita`, `eixos` e `motivo` acima do teto, `tt<n>`
  duplicado, argumento no limite do buffer (48 bytes crus, `\u00e9`,
  `\u0800`, `\ud83d\udc00` no fim), inteiro com fracao ou expoente
  (`1e2`, `1.5`) em campo de int e bytes crus invalidos em argumento
  (continuacao solta, prefixo sem continuacao, continuacao errada e
  DEL — ruim-265..268). E a rodada r2 (ruim-269..334): torto onde a
  leitura por peca nao descia — na 7a peca em diante e dentro de peca ja
  recusada por outro motivo — sempre `FCE_JSON`; `tela` e `origem` com
  tipo errado (`true`, numero, `\u0000`), `feita`/`perfil`/`v`/`vale_s`
  com fracao ou expoente, `v` negativo, referencias quase-boas em
  `itens`/`alvo`/`motivo` (`c1x`, `tt12a`, vazia, `c` sem digito, id
  longo demais), `confianca`/`passo` com `true`/`false`/`null`, `id` de
  selo fracionado, `k` de acao/info vazio, `x` com par de surrogatas no
  fim e a peca com todas as 22 chaves mais uma desconhecida. E a rodada
  r3 (ruim-335..384): raiz nao-objeto (`0`, `[]`, `"x"`, `true`,
  `null`), literais `true`/`null`/`false` e numero com fracao/expoente
  em campos do envelope (`v`, `tela`, `pecas`, `perfil`), numero torto
  no token (`1.:`, `1.a`, `1e:`, `1e+`, `1x`), `confianca` e selo `id`
  com fracao ou expoente, referencia `c<n>`/`tt<n>` seguida de pontuacao
  (`c/`, `c:`, `c.`, `c-`, `tt/`, `tt:`, `tt12a`), referencia acima do
  buffer (20+ caracteres, resolvivel ou nao), argumento de modelo
  `c<digitos>` fora da lista de candidatos (`c9`, `c011`, `c99999`,
  `c10`, `c90`), `\u0000` em `itens`/`alvo`/`id`/tipo da peca, `x` vazio,
  `id` de opcao vazio, `x` com surrogata solta e a cena cujo `confianca`
  tem numero torto.
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

## Cena inteira x por peca

**Documento que nao e JSON valido recusa a cena inteira** com `FCE_JSON`:
uma pre-passada estrutural confere a sintaxe do documento INTEIRO antes
de qualquer leitura por schema — todos os campos do envelope, todos os
niveis de array/objeto, as pecas ja recusadas e as pecas alem da 6a,
que a leitura por peca nunca alcanca. Entram aqui virgula ou
dois-pontos fora do lugar em qualquer nivel, `pecas` com elemento vazio
ou virgula sobrando, pecas sem virgula entre elas, string sem fecho,
escape invalido ou controle cru, branco que nao seja ` `/`\t`/`\n`/`\r`,
token que nao e numero/`true`/`false`/`null`, numero fora da gramatica
(`01`, `1.`, `1e`, `-`) e o `}` final trocado por outro byte. Um parser
JSON de verdade (o do servidor) reprova esses documentos antes de olhar
o schema — a TV faz o mesmo, entao `erro[]` nunca fica `FPE_OK` onde
houve recusa (ruim-269..294).

**A recusa por peca e so para JSON valido fora do schema**: tipo, chave,
enum, referencia, texto ou faixa errados derrubam aquela peca e as boas da
mesma cena seguem (chave duplicada conta aqui — o documento e JSON valido).
As pecas alem da 6a contam em `rec` sem serem lidas a fundo — mas sao
sintaticamente conferidas pela pre-passada: JSON valido a partir da 7a
posicao recusa so elas, JSON torto em qualquer posicao derruba a cena.

`\u0000` nao e aceito em nenhuma string: o codepoint 0 truncaria a string C
na validacao e o validador portado decidiria diferente — quem tem
`\u0000` cai como `FPE_X` (texto livre), `FPE_TEXTO` (id), `FPE_REF`
(referencia) ou `FCE_CAMPO` (envelope), conforme o campo.

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
de `motivo` e argumentos `c<n>` resolvem contra `cand` titulos. Um
argumento `c<digitos>` puro e referencia; `c<digitos><letras>` ou `"c"`
nao tem forma de referencia e vale como texto livre do modelo (secao
4.2 — boa-65..70), qualquer que seja `cand`.

`tests/farol_peca.c` roda o corpus inteiro; `tests/farol_peca.sh` compila e
executa sob ASan/UBSan.
