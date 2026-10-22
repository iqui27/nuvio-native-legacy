# Auditoria de pre-release

Um comando antes de toda release. Nasceu da 2.0.3, que chegou ao fim da
integracao com o Continuar e os Salvos perdendo filme em andamento, a pagina da
serie sem episodios quando aberta pela ilha ou pelo player (e sem cartao de
proximo episodio), um marcador "credits source=introdb" que nao vinha do
TheIntroDB e dado sumindo na troca de perfil. Nenhum teste de `tests/` pegava.

```bash
export TMPDIR=/Volumes/ExternalSSD/tmp
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
bash tools/auditoria-release.sh              # ~3 min: estatica 20 s, testes 20 s, build 45-70 s, smoke ~2 min
bash tools/auditoria-release.sh --sem-smoke  # so estatica + testes (~40 s)
bash tools/auditoria-release.sh --sem-build  # smoke com o binario ja compilado
bash tools/auditoria-release.sh --completo   # mais salvospainel, cw_pedidos e detail_remonta (~50-70 s cada)
```

Sai com 0 so com todos os checks executaveis PASS. "NAO VERIFICADO" conta
como falha no resumo (por exemplo, outro app aberto ou perfil indisponivel).
INFO explicita limites fora do escopo do smoke, sem afirmar cobertura real.
Logs e capturas em `$TMPDIR/nv-auditoria*`.

## 1. Estatica (`tools/auditoria/estatica.py`)

Grafo de chamadas refeito a cada execucao pelo extrator AST do graphify sobre
`src/` da arvore auditada (cache em `$TMPDIR/nv-auditoria-ast`). Dois cortes
para o grafo nao mentir: aresta para `static` de outro arquivo sai (o graphify
liga pelo nome), e aresta para funcao de outro arquivo que nao esta em nenhum
`.h` tambem (era o `stat()` da libc casando com uma funcao `stat` do projeto).
`pthread_create`/`SDL_CreateThread` viram arestas marcadas como entrada de fio.

| # | Verifica | Falha quando |
|---|----------|--------------|
| 1 | Toda entrada de pagina/player pede o proprio dado | `detail_abrir`, `player_abrir`, `player_retomar_retido` nao alcancam `desc_episodios` no grafo (o pedido fica num diff por quadro do roteador, com portao); alguem abre o detalhe por fora do funil `abrirTitulo` |
| 2 | O que a troca de perfil derruba volta | um `*_esquecer/limpar/zerar` do tratador da troca sem nenhum refazer do mesmo modulo alcancavel; setter zerado com `NULL` sem chamada com dado alcancavel; publicador de dado do perfil num fio sem conferir perfil/geracao |
| 3 | Rotulo de fonte so vem de campo real | a loja por tras de um rotulo (`trechos` -> "introdb") e escrita por leitor de outra API; rotulo "por eliminacao" num ternario sai como aviso |
| 4 | Estado de arquivo escrito por 2+ fios sem trava | estatica escrita sem trava pelo fio e pelo quadro (trava no chamador conta; `volatile`, atomico e `_Thread_local` ficam fora) |
| 5 | Rede/espera no fio do quadro | `rede_*`/curl/`sleep` alcancavel de `app_atualizar/app_desenhar/app_evento` sem passar por fio; `pthread_join` e aviso; portao de teste (flag so ligada por funcao sem chamador) e ignorado |
| 6 | Helper duplicado | mesmo corpo (>= 160 caracteres) em 2+ arquivos |
| 7 | `tests/*.sh` que cala o status do teste | pipe cujo lado esquerdo e o binario do teste, compilador ou interprete (`./bin \| tail`, `\| tee`, `\| grep`) em script sem `set -o pipefail`/`set -euo pipefail`: o status vira o do `tail`/`grep` e um teste que falha sai 0. Lista `arquivo:linha`; sem base, e contrato |

Os contratos (nomes de funcao) ficam no topo do script. Renomeou a funcao? A
verificacao falha dizendo "atualizar ENTRADAS" em vez de passar calada.

**Divida conhecida (4, 5, 6).** `tools/auditoria/base.txt` lista o que ja
existia quando a auditoria nasceu; so o que for NOVO falha. Consertou um item?
Apague a linha. `--sem-base` mostra tudo. As verificacoes 1-3 nao tem base:
falha ali e defeito de produto.

## 2. Testes focados

`cwordem cwlocal cwretido cwfrente cwoculto detail_eps episodiosdup proximo
credfonte intro perfilcont perfilsel salvos progresso syncprog` (`--completo`
acrescenta `salvospainel cw_pedidos detail_remonta`).
Todos passavam na 2.0.3 com os defeitos dentro — por isso as outras duas
partes existem.

## 3. Smoke do app real no Mac (`tools/auditoria/smoke_mac.py`)

Compila com `tools/mac.sh` (`NUVIO_MAC_BIN` = binario proprio, nao o
`/tmp/nuvio-native-legacy-mac` de quem estiver usando), copia `~/.nuvio` para
`$TMPDIR/nuvio-auditoria-dados` (sem `cache/` e EPG; as telas de novidades
marcadas como vistas NA COPIA) e roda duas vezes:

- **A** (perfil ativo): Continuar nao vazio; todo item em andamento do perfil
  no `progresso.txt` (1-90%, >= 60 s, fora do `cwoculto.txt`) mais recente que
  o ultimo card esta na fileira; `abrir:<serie>` mostra episodios; "Cima" na
  pagina abre os Salvos com linhas; a MESMA serie reaberta mostra episodios.
- **B** (troca): Direita + OK na escolha de perfil; a home do perfil novo
  monta e o Continuar e dele, nao do anterior.

O que o smoke le no log: `[desc] cw[i]` (com `NUVIO_CW_LOG=1`),
`[detail] episodios na pagina: <id> <n>` e `[spainel] aberto: <n> salvos` —
as duas ultimas linhas existem para isto.

Regras: nunca aperta Reproduzir (o unico OK e na tela de escolha de perfil, e
so depois de o log anunciar a tela); nunca escreve em `~/.nuvio`; so mata o
binario que ele mesmo abriu; com outro app do Mac aberto sai "NAO VERIFICADO"
(as portas `/tmp/nuvio-key` e `/tmp/nuvio-shot-req` sao do sistema todo).

Limites honestos: o caminho "ilha -> pagina" e "player -> pagina" exige ter
tocado algo, entao aparece como INFO de cobertura: a abertura real nao e
exercitada; a estatica 1 confere que essas entradas pedem episodios. Antes
este limite fixo era NAO VERIFICADO e tornava PASS impossivel mesmo com todos
os checks passando. Checks executaveis indisponiveis continuam saindo com 2. A
copia usa a sessao da conta do dono: o app pode renovar o token na copia, e o
refresh token antigo (o de `~/.nuvio`) pode deixar de valer — se o app do Mac
pedir login depois de uma auditoria, e isso.
