# Issues do GitHub, mapeados um por um

Os dez issues abertos em `iqui27/nuvio-native-legacy`, mais o que veio pelo
Reddit. Cada entrada diz **o que a pessoa viu**, **a causa encontrada no
codigo** e **o que foi feito**. Onde a causa nao pode ser provada sem o
aparelho, esta escrito que nao pode — e o que foi feito no lugar.

A regra deste arquivo: nada de "provavelmente". Ou a causa esta apontada com
arquivo e linha, ou esta marcada como NAO CONFIRMADA.

---

## #1 — poster dont load on tiles (Tizen) · @rawldon

Cartazes falham **as vezes** dentro dos cards; o card aparece, a imagem nao.

**Causa, em `src/tex_cache.c`.** Um erro de contagem que deixava o proprio
recuo pela metade. A guarda de reenfileiramento era `itens[i].falhas < 3` e a
tabela de recuo e `RECUO[] = {2 s, 10 s, 60 s}`, indexada por `falhas` **antes**
do incremento. Ou seja: a terceira falha gravava `tentarEm` para 60 s adiante e
a guarda ja nao deixava aquele prazo ser usado nunca. Os 60 s eram codigo morto,
e o item ficava **para sempre** sem arte — ate outro card reaproveitar o slot
por `slotLivre()`.

Por que isso aparece mais no Tizen: la todo download passa pela pilha HTTP do
navegador e dezenas de imagens saem ao mesmo tempo no arranque. Uma rajada de
falhas ali condenava aqueles cartazes pelo resto da sessao — que e exatamente o
"as vezes" do relato.

**Feito.** Teto de tentativas removido. O recuo continua e fica preso em 60 s
depois das duas primeiras tentativas, entao uma URL morta custa **um pedido por
minuto** enquanto o card estiver na tela, e uma falha de rede se recupera
sozinha.

**Feito no v1.0.15 — era WebP mesmo.** A foto do log de uma QN90A fechou:
`[tex] decode falhou (Unsupported image format) tam=14312 magica=52494646`, e
`52494646` e `RIFF`. Eu tinha descartado a hipotese antes testando so o
Metahub, que devolve JPEG independente do `Accept`, e generalizei errado.

`src/webp.c` so sabia `dlopen` da libwebp do aparelho, e nao existe `dlopen` em
WebAssembly — no Tizen ele devolvia NULL calado desde sempre. O Emscripten
tambem nao tem port de libwebp, entao `-sSDL2_IMAGE_FORMATS` segue com png e
jpg (`tools/tizen.sh`), e os selos do pacote seguem convertidos para png por
`tools/tizen-art.sh`.

A saida foi a que ja estava anotada aqui: **decodificar pelo proprio
navegador**. O fio de decode passa os bytes ao fio principal, que dispara
`createImageBitmap` e volta ao laco de quadro; o resultado atravessa um canvas
2D, vira RGBA e um `Atomics.notify` acorda o fio de decode, que dormia num
futex. Futex e nao asyncify (unwinding dentro de Worker de pthread, centenas de
vezes por sessao); fio principal e nao worker (worker nao tem `document`).

Como conferir na TV: painel de log (vermelho) e procurar
`[webp] navegador decodificou o primeiro: LxA`. Regressao em
`tests/webp-tizen.sh`, que precisa de navegador — Node nao tem
`createImageBitmap`.

---

## #2 — Main Nuvio icon not showing in apps list (LG NanoCell 49SM9000PLA) · @Haylefal

Icone em branco no menu de apps, e continua em branco depois de reiniciar.

**Causa, provada por inspecao do arquivo.** `deploy/app/icon.png` era um
**quadrado chapado**: 80x80, PNG valido, e a imagem descompactada tinha
**quatro valores de byte distintos** — cor `#141418` e nada mais. O
`appinfo.json` apontava `icon` e `largeIcon` para esse mesmo arquivo. O icone
oficial existia, mas so no alvo Samsung (`deploy/app/tizen/icon.png`, 512x423,
adicionado no commit 5148bd5 junto do "icone oficial"); o webOS ficou com o
espaco reservado.

**Feito.** Icone real gerado a partir do logo oficial, recortado em quadrado
centrado na marca: `deploy/app/icon.png` 80x80 e `deploy/app/icon-large.png`
130x130, que sao os dois tamanhos que o webOS pede. `appinfo.json` aponta o
grande para o arquivo grande, e `tools/arm.sh` passou a mandar o arquivo novo no
deploy incremental (a lista era `art fonts icon.png`).

**Atencao no teste:** o launcher do webOS **cacheia o appinfo** — ja documentado
em `FERRAMENTAS.md`. Icone novo pode exigir desinstalar e instalar de novo, nao
so relancar.

---

## #3 — Language still showing spanish (na verdade: portugues) · @Haylefal

Interface em ingles, mas a Biblioteca continua em portugues: "0 titulos", "Sua
lista para assistir".

**Causa, em `src/biblioteca.c:420`.** A traducao deste app mora num ponto so:
`src/text.c` chama `i18n()` em toda linha desenhada, e a chave da tabela e o
portugues **inteiro** da string. O resumo da biblioteca e montado com
`snprintf`, entao a string que chegava em `text.c` era
`"0 títulos   ·   Sua lista para assistir"` — que nunca vai existir como chave.
As quatro partes ja existiam traduzidas em `src/idioma_tab.h` (linhas 337, 339,
414, 415): a tabela estava certa, o ponto de chamada estava errado.

**Feito.** `i18n()` aplicado nas **partes** antes de montar a frase. Mesmo
tratamento em `src/ajustes.c` para `"Perfil %d"` e `"Conectar %s"`, com os pares
novos em `idioma_tab.h`. O rodape `"PgUp / PgDn  Trocar seção"` tambem ganhou
par (e o atalho em si e tratado no issue #7 da lista de pedidos, porque **nao
existe PgUp num controle de TV**).

---

## #4 — Nuvio Keyboard Help · @Gustavo-PGM

No teclado da busca, cima/baixo "pula para uma letra aleatoria". Com video.

**Causa, em `src/focus.c`.** `focus_mover()` implementa **memoria de coluna por
fileira**, que e o comportamento certo para fileiras de conteudo (cada uma tem
comprimento proprio e a pessoa guarda o lugar em cada). Numa **grade** e um
defeito: descer de "f" (coluna 5) ia para `colunaLembrada[fileira seguinte]`,
que e 0 se aquela fileira nunca foi visitada — ou seja, para o "g" em vez do
"l". Descer de novo: "m". Voltando, o cursor reaparece em "f". Do sofa isso le
exatamente como "pula para uma letra aleatoria".

**Feito.** `focus_mover_grade()` novo em `src/focus.c`: preserva a coluna e
apenas prende ao fim da fileira de destino quando ela e mais curta (a ultima
fileira do teclado tem 3 teclas, nao 6). Usado no teclado da busca
(`src/busca.c`) e tambem na **grade de posteres da biblioteca**
(`src/biblioteca.c`), que tinha o mesmo defeito pelo mesmo motivo. As fileiras
de conteudo (home, detalhe, resultados da busca) continuam com memoria de
coluna, que la e o comportamento desejado.

---

## #5 — Nuvio Sync Issue · @Gustavo-PGM

Duas metades: (a) o que ele assiste no celular nao aparece em "Continuar
assistindo" na TV; (b) aparecem filmes e series que ele **nunca assistiu**.

**Causa, em `src/descoberta.c`.** A fileira sai **exclusivamente do Trakt**
quando o Trakt esta vinculado:

```c
nContinuar = trakt_continuar(lote, 8);
if (nContinuar == 0 && !trakt_ativo()) nContinuar = continuarLocal(lote, 8);
```

O progresso da **conta Nuvio** — que e o que chega do celular, por
`src/syncprog.c` em cima de `src/progresso.c`, e e o que `continuarLocal` le —
era simplesmente ignorado nesse caso. Isso explica as duas metades de uma vez: o
que veio do celular nao entra, e o que entra e o `/sync/playback` do Trakt, que
guarda retomadas antigas de qualquer aplicativo. O caminho do Trakt tambem nao
aplicava os limites de 1% a 90% que `continuarLocal` aplica.

**Feito.** `montarContinuar()` em `src/descoberta.c` une as duas fontes,
deduplica por imdb (a obra, nao o episodio) e ordena por instante. Os limites de
**1% a 90% agora valem para as duas** — o caminho do Trakt nao os aplicava, e
era dali que vinham os titulos em 0% e os praticamente terminados.

**A ordem exigiu uma peca a mais.** `trakt_continuar` lia o `progress` e
descartava o `paused_at` do `/sync/playback`, entao todo item do Trakt entrava
com instante desconhecido e a fileira ordenava pela ordem de resposta. Agora o
`paused_at` e lido e viaja **no proprio item** (`CatItem.retomadoMs`) — e nao num
vetor paralelo, porque `trakt_enfeitar_lote` **compacta** o lote ao tirar o que
o Cinemeta nao conhece, e um vetor indexado por posicao dessincroniza ali em
silencio.

O parser de data virou compartilhado no caminho: era `isoParaMs`, privado em
`src/syncprog.c`, e agora e `js_ms_iso` em `src/js.c`. Duas copias de um parser
de data divergem, e divergem **em silencio** — o sintoma seria uma ordem errada,
nao um erro.

---

## #6 — Video Black Screen Issue · @Gustavo-PGM

Em alguns titulos o audio toca, a barra de tempo anda, os menus respondem — e a
tela fica preta. Trocar de fonte nao resolve.

**Nao da para detectar, e isso ja foi MEDIDO neste aparelho.** Esta escrito em
`src/video.c`, no lugar onde um recuo automatico por prazo foi removido: o uMS
reporta `videoInfo`, `sourceInfo` e `loadCompleted` **normalmente** nos arquivos
que ficam sem imagem. Caso registrado: `videoInfo 3840x1606
hdrType=DolbyVision`, `loadCompleted` em 3212 ms, tela preta com o audio
correndo. **Nao existe no uMS sinal de quadro exibido** — o `currentTime` avanca
puxado pelo audio. No webOS `vidW` ainda nasce em 1920x1080, entao nem a
dimensao serve de pista.

O que o codigo ja cobre sao os casos em que o **pipeline desmente a fonte**:
pedimos DolbyVision e o `hdrType` volta `HDR10` (recuo para HDR10) ou `none`
(recuo para SDR). O caso que sobra e o pior: o pipeline **confirma**
DolbyVision, o ACB aceita, e nao ha quadro. Recuar automaticamente ali quebraria
o Dolby Vision que funciona de verdade nos MP4 perfis 5 e 8.

**Feito — saida manual, porque o app nao pode adivinhar.** `video_forcar_sdr()`
recarrega a **mesma fonte, na posicao atual, sem afirmar HDR nenhum**, e a
escolha vale pela sessao (uma recuperacao automatica posterior nao traz o DV de
volta pelas costas). Fica exposto como um terceiro botao **"Sem HDR"** no
cabecalho da folha de Fontes — que e onde a pessoa com tela preta ja vai. A
linha de contexto do painel explica o botao quando ele esta em foco:
_"Imagem preta com o áudio tocando? Recarrega esta fonte sem HDR nem Dolby
Vision."_

`video_pode_forcar_sdr()` responde 0 no Tizen e no Mac: la o HDR e decidido pelo
AVPlay do firmware e nao ha o que renegociar, e o botao nem aparece — melhor que
um botao inerte.

**Ainda em aberto:** se o relato do Gustavo for de tela preta **no Tizen**, esta
correcao nao o alcanca. Precisamos saber em qual aparelho ele esta.

---

## #7 — UI Resolution & Torrent Menu Lag · @Gustavo-PGM

Tres sintomas juntos: texto "pixelado" na tela de perfil; na tela de escolher
fonte **todo o texto aparece em negrito** e "pixelado"; e essa tela em
particular fica **muito lenta**.

**Causa, em `src/text.c`.** `fonteDe()` manda a **linha inteira** para a fonte
de reserva quando o primeiro caractere fora do ASCII nao existe na Inter. A
regra e certa para **escrita** — "Deadpool & ウルヴァリン" sai legivel na
DroidSansFallback — e errada para **simbolo**: **um** simbolo que a Inter nao
tem bastava para a lista de fontes inteira trocar para a DroidSansFallback, que
e uma fonte CJK, cujo latim e mais pesado e com outro hinting. Isso e
literalmente "todo o texto em negrito e pixelado". E era tambem trabalho a mais
— abrir um segundo arquivo de fonte por estilo e rasterizar com uma fonte muito
maior — na mesma tela que o relato descreve como lenta.

**Cobertura da Inter, medida** com `TTF_GlyphIsProvided` nos tres pesos
embarcados, porque a lista de culpados importa:

| | |
|---|---|
| **tem** | `← ↑ → ↓` `•` `…` `–` `—` `" '` `★` `▶` `✓` `·` |
| **nao tem** | `⚡` `⚙` `⭐` |

Quem derrubava a linha eram os que ela **nao** tem — e `⚡` e o mais comum nos
nomes do Torrentio e do AIOStreams. As setas e o `▶` que a **propria interface**
usa nos seus rotulos sempre passaram.

Os emoji tinham o **outro** sintoma, nao este: `fonteDe` devolve a fonte
principal para codepoint fora do BMP (`TTF_GlyphIsProvided` recebe `Uint16` e
nao os alcanca), entao `💾` e `🇧🇷` nunca trocaram a fonte da linha — saiam como
o retangulo do `.notdef`.

**Feito.** Os codepoints **decorativos** (simbolos, setas, pictogramas, emoji,
seletores de variacao) que a fonte principal **nao tem** sao retirados da linha
antes de rasterizar — e so eles. O que a fonte tem fica: en-dash, reticencias,
aspas curvas e o proprio `·` estao na Inter e passam intactos, porque a decisao
e por glifo disponivel e nao por faixa. `"⚡ 1080p · 4.2 GB"` vira
`"1080p · 4.2 GB"`, na Inter, sem quadradinho. Escrita de verdade nao e tocada:
japones, cirilico e arabe continuam caindo na reserva certa.

Cobre tambem a tela de perfil, se o nome do perfil tiver emoji — que e o caso
comum de um perfil de familia.

**Nao confirmado:** a parte "lenta" pode ter uma segunda causa. O rasterizador
tem orcamento de **duas linhas novas por quadro** (`TXT_POR_QUADRO` em
`src/text.c`) e cada linha da folha de fontes desenha cinco textos, entao encher
o painel leva ~13 quadros por construcao. Se ainda estiver lento depois desta
correcao, o numero a medir e `txt_despejos`.

---

## #8 — Profile Switching & Metadata Language Bug · @Gustavo-PGM

Duas coisas: (a) "Trocar perfil" **abre um filme aleatorio** de Continuar
assistindo em vez de trocar de perfil; (b) titulos e detalhes continuam em
ingles com o app em portugues.

### (a) Causa, e ela e determinista

A barra lateral decide no **KEYDOWN** (`src/menu.c`, `escolher()`) e se fecha
ali mesmo. A home age no **KEYUP** do OK (`src/home.c`, e ela age no KEYUP de
proposito, porque so a subida da tecla conhece a DURACAO, que e o que separa
"abrir" de "segurar para o menu do cartaz"). Entao, no **mesmo toque**:

1. KEYDOWN → `menu_aberto()` e 1 → `menu_evento` → `escolher()` → pede a troca
   de perfil e fecha a barra.
2. KEYUP → `menu_aberto()` agora e **0** → o roteador de `src/app.c` entrega o
   evento a home → a home abre o card em foco.

O foco da home costuma estar na primeira fileira, que e "Continuar assistindo" —
daí "abre um filme aleatorio de Continuar assistindo". O pedido de abrir fica
pendente, o app vai para a tela de perfis, volta para a home, e o pedido
dispara.

`src/detail.c` **ja tinha** essa guarda, com um comentario descrevendo o mesmo
defeito ("clica num titulo e ele ja clica duas vezes e inicia"); `src/ctxmenu.c`
tambem. A home era a que faltava.

**Feito.** A home ignora um KEYUP de OK cujo KEYDOWN ela nao viu. Vale para todo
item da barra lateral e para o OK que fecha qualquer folha desenhada acima da
home.

### (b) Metadados em ingles — causa diferente da suposta

`desc_tmdb_idioma()` (`src/descoberta.c:1679`) devolve `pt-BR` quando a
interface esta em portugues, e `src/extras.c`, `src/pessoa.c` e `src/diretor.c`
**ja pedem** com `&language=`. O que **nao** e localizado e o **titulo**: ele vem
do catalogo do addon (Cinemeta e afins), que responde em ingles, e o app nunca
pede a versao localizada ao TMDB para os itens de catalogo. Nao e um ajuste que
nao funciona; e um caminho que nao existe.

**Feito, para a pagina de titulo.** `fotosDoElenco` ja resolvia o id do TMDB
pelo `/find`; agora pede tambem `/{tipo}/{id}?language=<pref>` e substitui
titulo e sinopse. **Um** pedido a mais numa funcao que ja fazia tres, e ela roda
uma vez por titulo **aberto**, num fio proprio — nao por card da home.

Duas decisoes que o codigo registra:

- **`js_texto_raiz` e nao `js_texto`.** Numa resposta `/tv` o `"name"` aparece
  dentro de `created_by[]`, `genres[]`, `networks[]` e `seasons[]`, e os
  primeiros vem **antes** do `"name"` da raiz na ordem que o TMDB emite: a
  leitura crua traria um **genero** no lugar do titulo da serie. O leitor de
  chave de profundidade 1 nasceu para o `id` do manifesto Stremio (issue #10) e
  virou compartilhado aqui — segundo consumidor, mesma regra do parser de data.
- **Campo vazio nao substitui.** O TMDB responde 200 com `""` no titulo e na
  sinopse quando ninguem traduziu aquele filme; trocar o ingles por vazio
  deixaria a tela **sem** titulo. Falta de traducao mostra o original.

**O que continua em ingles:** o titulo nos **cards da home**. Ele vem do
catalogo do addon, e Cinemeta e afins nao tem parametro de idioma — localizar
cada card custaria um pedido ao TMDB **por card**. Trocar o provedor de
metadados (o que o "plugin TMDB" do app web faz) e outra ordem de trabalho.

---

## #9 — Missing Subtitle Languages Bug · @Gustavo-PGM

So aparece legenda em ingles; as outras opcoes desapareceram e mudar nos
ajustes "nao muda nada". A fonte tem varias legendas no PC e no celular.

**Duas causas, as duas confirmadas no codigo.**

1. **Mudar o ajuste nao refazia a busca.** `addons_buscar_legendas()`
   (`src/addons.c`) declina quando o id pedido e o que ja esta em memoria — e a
   decisao certa, senao cada quadro do detalhe refaria dezenas de requisicoes.
   Mas a lista depende **tambem** do idioma preferido (`gruposIdioma`), e trocar
   o idioma nao mudava id nenhum: a folha continuava mostrando exatamente o que
   o idioma anterior deixou. Ou seja, o ajuste era real e **inobservavel**.

2. **A lista de idiomas oferecida era um recorte.** `OPCOES_COD` em
   `src/linguas.c` tinha doze idiomas e parava em `hi` — enquanto a tabela de
   nomes do mesmo arquivo cobre 28 e `ling_casa()` aceita todos. E o mesmo
   defeito do relato sueco do Reddit (abaixo). Pior: `LING_MAX_OPC` em
   `src/ajustes.c` era 24 e **truncava em silencio**, entao crescer a lista sem
   crescer o teto nao teria efeito.

**Feito.** `addons_legendas_reiniciar()` novo: descarta a lista, zera o alvo
para desarmar a guarda e refaz a busca do titulo carregado. Chamado quando a
preferencia muda — com **repouso de 0,7 s**, porque a linha de Ajustes roda o
aplicador a cada toque de esquerda/direita, a lista agora tem trinta idiomas, e
no controle da TV a seta repete sozinha: sem o repouso, atravessar a lista
dispararia dezenas de buscas em todos os addons de legenda.
`OPCOES_COD` passou a cobrir a tabela inteira (30 opcoes: "Da conta", "Todas" e
28 idiomas) e `LING_MAX_OPC` foi para 32.

**Nao explicado por estas duas:** se a fonte dele tem legendas **embutidas** em
varios idiomas e so a inglesa aparece, o problema esta na enumeracao de faixas
do pipeline (`video_n_legenda`), nao no filtro — o filtro por idioma nao e
aplicado as embutidas. Isso precisa do video dele para separar os dois casos.

---

## #10 — Bug: Collection is completely empty on Tizen (UA65AU7000KXXA, Tizen 6.0) · @rawldon

Abrir uma colecao mostra **nada**: nem cartaz, nem card.

**Tres causas encadeadas, todas em codigo.**

1. **`collections.json` nao existe no pacote Tizen.** `tools/tizen-art.sh`
   escolhe o subconjunto da arte que pode ser distribuido e copia apenas
   `icones badges marcas prov editorial logo poster ep elenco` mais os `.jpg`
   da raiz. O `collections.json` fica de fora — e isso esta **certo**: o arquivo
   guarda as URLs de addon de quem empacotou, com o token embutido no caminho.
   Consequencia: no Tizen as colecoes existem **so** pela conta.
2. **Uma fonte vinda da conta guarda `addonId`, nao URL.** A URL sai de
   `addons_base_por_id()`, que so conhece o id depois que a sonda leu o
   manifesto daquele addon. E `capacidadesDoManifesto()` (`src/addons.c`)
   **comecava** com `if (!r) return;` sobre `"resources"` — um campo que o
   protocolo pede mas que addon real as vezes omite ou escreve de forma que este
   leitor nao alcanca. Nesse caso o addon ficava **para sempre** sem id, e a
   pasta abria vazia. Alem disso `addons_definir_lista()` nunca zerava o campo
   `id` do slot: numa segunda chamada — e ela acontece a cada ciclo de sync — o
   slot herdava o id do addon que estava **antes** naquela posicao, com uma base
   diferente, e o mapa passava a devolver a base **errada**.
3. **Base vazia virava um pedido relativo.** `desc_vertudo_filtro()` so recusa
   ponteiro nulo, e a base e um vetor dentro da `ColSource` — nunca nula, as
   vezes vazia. Com ela vazia a URL montada era `/catalog/movie/<id>.json`, um
   caminho **relativo**: no Tizen o XHR resolve isso contra a origem do proprio
   widget, a resposta e o `index.html`, e nada decodifica. Tela em branco, sem
   erro.

**Feito.**
- O `id` e o `name` do manifesto passaram a ser lidos **antes** de qualquer
  retorno cedo, e o id sai de um leitor que anda pelas chaves de **profundidade
  1**: `js_texto` para na primeira ocorrencia de `"id"` no documento, e um
  manifesto Stremio tem `"id"` tambem dentro de `catalogs[]` e de
  `behaviorHints` — quando o autor poe `"catalogs"` antes de `"id"`, a leitura
  crua devolvia o id de um **catalogo** como se fosse o do addon. Oito casos
  cobertos por teste, incluindo `"id"` escapado dentro de uma descricao.
- `addons_definir_lista()` zera a entrada inteira.
- `src/vertudo.c` nao dispara pedido com base vazia, **comeca na primeira aba
  que tem endereco** (numa pasta com varias abas e comum que so parte dos
  addons esteja instalada aqui), reconfere por quadro enquanto estiver nesse
  estado — assim a colecao se preenche sozinha quando o manifesto responde, em
  vez de exigir sair e entrar — e a tela **diz** o que falta:
  _"O addon desta coleção não está instalado nesta TV."_

---

## Reddit

### Colecoes sem arte nos cards (Tizen)

Mesma cadeia do #10. No Tizen a pasta vem da conta, entao a capa e a
`coverImageUrl` crua do CDN em vez da arte curada do pacote — e se essa URL for
**webp**, ela ja decodifica desde o v1.0.15 (ver a nota em #1). O `col_definir_json`
ja preserva a arte do pacote para as pastas que existem nos dois lados; no
Tizen nao existe lado do pacote, porque o `collections.json` nao vai no `.wgt`.

### Legenda em sueco nao aparece nos ajustes

Causa 2 do #9. Corrigido: `sv` esta entre as 28 opcoes agora.

### App nao traduzido para sueco

**Nao feito, e e trabalho de outra ordem.** A traducao deste app tem **dois
idiomas** por construcao: a chave da tabela e o proprio portugues e
`ajustes_idioma_ingles()` e um booleano (`src/idioma.h` explica por que nao ha
catalogo de simbolos). Um terceiro idioma exige trocar o booleano por um indice
de idioma e a tabela de pares por uma tabela por idioma — mudanca contida, mas
que muda a forma do sistema, e 426 strings para traduzir por idioma novo.

### Catalogos nao carregam (Tizen)

Sem mais detalhes do relato, o candidato de codigo e a causa 2 do #10 (addon sem
`id` e sem capacidades lidas do manifesto: `capacidadesDoManifesto` retornava
antes de marcar `sondado`, e um addon nao sondado opera na suposicao otimista).
**Feito o que se podia fazer sem o aparelho: instrumentar e consertar o que a
leitura mostrou.** `lerCatalogo` passou a distinguir **"vazio hoje"** de **"nao
respondeu em 8 s"** — os dois saiam como o mesmo zero. A linha de resumo agora
responde a pergunta:

```
[desc] catalogos: N pedido(s) em R rodada(s), X responderam, Y sem resposta,
       Z vazio(s), W repetido(s); F de T fileira(s) no limite
```

Dois defeitos reais apareceram nessa passagem:

- **Fileira duplicada.** A mesma chave `<addonId>_<tipo>_<catId>` duas vezes
  (addon repetido na lista da conta, ou manifesto que declara o catalogo duas
  vezes) virava a mesma fileira em duplicata. Agora e pulada e contada.
- **Catalogo vazio custava uma fileira.** O lote pedia exatamente o teto de
  fileiras, entao cada catalogo que respondesse vazio deixava um buraco:
  escolher 7 e ver 5. Agora o pedido acontece em **rodadas** — a seguinte pede
  so o que faltou —, e o caso comum continua sendo uma rodada.

**O suspeito mais forte, medido por leitura e nao consertado:** em `montar`,
`trakt_continuar` (timeout 25 s) e `trakt_social` (25 s) rodam **em serie** no
fio da descoberta **antes** de qualquer manifesto, e `lerManifesto` roda em
serie por addon, com 20 s cada. Com 4 addons e um servidor lento, o pior caso
passa de 100 s antes do primeiro GET de catalogo. Nao foi paralelizado porque
`desc_alvo_busca()` escreve em `alvos[]`/`nAlvos` **sem trava** (fios ali
corromperiam estado) e porque a ordem em que os `Decl` entram define a ordem das
fileiras. Com a instrumentacao nova o log passa a dizer onde o tempo vai; vale
como tarefa propria.

---

## Fora dos issues, e que sai na mesma leva

### Segredo no log, cortado na ORIGEM

Achado enquanto o painel de log era construido. A chave do debrid viaja no
**caminho** das URLs de addon (`https://host/manifest/<id>/<jwt>`,
`https://host/d/<chave>/arquivo.mkv`) e o app imprimia essas URLs com `%.60s` —
o bastante para incluir o segmento com a chave — em `src/debrid.c` e em quatro
pontos de `src/rede.c`. O destino desse texto deixou de ser um arquivo de
desenvolvimento: no webOS vai para `/tmp/nuvio.log`, legivel por qualquer
processo; no Tizen vai para o console do navegador e, com `NUVIO_LOG_URL`
ligado, **sai do aparelho pela rede**; e agora aparece **na tela da TV**.

O painel corta a URL na exibicao — mas cortar so na exibicao protege a sala e
nao o arquivo. Agora corta na origem: `rede_url_publica()` deixa esquema e host
(o que interessa para saber qual addon respondeu) e substitui o caminho por
`/...`. Testado sob ASan/UBSan em 10 formas reais de URL, inclusive a chave de
API do TMDB na query string, que tambem passa a ficar fora do log.

### Pedidos novos do dono, na mesma sessao

- **Ajustes das fileiras da Home**: limite (default **7**, faixa 3–16), ordem,
  liga/desliga por fileira e forma/tamanho de card por fileira. A escolha e
  **local e nunca sobe para a conta** — a trava esta escrita em `catordem.h` e
  repetida no modulo novo (`src/fileiras.c`): a lista que a TV consegue montar e
  menor que a real sempre que um addon demora, e empurrar apagaria a
  configuracao da pessoa nos outros aparelhos.
- **Navegar entre categorias dos Ajustes**: `PgUp`/`PgDn` estava escrito no
  rodape e **nao existe em controle de TV** — o atalho era inalcancavel no
  aparelho. Agora Voltar e um nivel de hierarquia (lista → indice de categorias
  → sair), com a coluna de secoes sempre visivel e focavel, e o rodape diz as
  teclas que funcionam.
- **Painel de log pela tecla vermelha**, nos tres alvos (`src/registro.c`), e um
  aviso de primeira execucao que ensina o gesto **por fabricante**. O scancode
  da vermelha no webOS saiu do `SDL_webOS.h` do sysroot, nao de chute: RED 486,
  GREEN 487, YELLOW 488, BLUE 489. **Nao confirmado:** se a vermelha chega ao
  app na LG — o Back precisou de um hint proprio para nao ser engolido pelo
  compositor e nao existe hint equivalente para as coloridas.
- `FERRAMENTAS.md` tinha a secao "Tecla Back" **desatualizada**: descrevia o
  truque do par `FOCUS_LOST`/`FOCUS_GAINED`, que nao existe mais no codigo desde
  que o hint de access policy entrou. Quem fosse depurar o Back por ela ia
  procurar no lugar errado. Reescrita, com o "como era antes" preservado.

---

## O que nao pode ser verificado daqui

- **Nada acima foi visto rodando numa TV.** O que foi verificado:
  - compilacao limpa nos **tres alvos** — `cc -fsyntax-only` no Mac, `emcc` do
    emsdk (que e o unico que compila o bloco `EM_JS` da rede) e
    `arm-webos-linux-gnueabi-gcc` dentro do `nuvio-webos-sdk` (o unico que
    compila o ramo do pipeline de video) — mais o **link** do alvo Mac;
  - testes de unidade sob ASan/UBSan: leitor de chave de raiz (8 casos de
    manifesto + 9 de resposta TMDB), filtro de glifos decorativos (9 casos),
    mascara de URL (10 casos), parser de data ISO (8 casos);
  - cobertura de glifos da Inter medida com `TTF_GlyphIsProvided` nos tres pesos;
  - ordem da tabela de traducao (**476** entradas) conferida pelos **bytes
    decodificados**, que e o que `strcmp` compara em `idioma.c` — o script de
    manutencao ordenava pelo literal, e isso ia sair errado assim que entrasse
    uma chave com `\"` ou `\n`, que e o caso das novas.
- **#6 no Tizen**, **#9 nas legendas embutidas** e a parte **"lenta"** do #7
  precisam do aparelho ou do video de quem relatou.
- **Se a tecla vermelha chega ao app na LG** — ver acima.
