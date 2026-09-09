// Catalogo de titulos vindo de um arquivo, no lugar das listas fixas no codigo.
//
// Existe porque testar layout com nomes inventados esconde problemas reais: os
// titulos de verdade tem tamanhos muito diferentes ("CODA" contra "Assassinos
// da Lua das Flores"), acentos, e sinopses que nao cabem em tres linhas. Cada
// item tambem carrega o LOGO do titulo, que e o que o app da Apple desenha no
// lugar do nome em texto.
#ifndef NV_CATALOGO_H
#define NV_CATALOGO_H

// 40 titulos hoje (14 do historico do dono + 26 dos catalogos). A folga evita
// o corte silencioso que ja aconteceu: com 32 os oito ultimos sumiam sem aviso.
// Nao ha mais teto de catalogo: o vetor cresce conforme a rede entrega. Um
// numero fixo aqui sempre foi arbitrario — comecou em 32, virou 48, 160, 260,
// e a watchlist do dono continuava batendo no limite. O que limita de verdade
// e o cache de TEXTURA, que tem teto proprio e so guarda o que esta na tela;
// o item em si custa ~3,5 KB de texto.
//
// CAT_MAX sobrevive so como teto de seguranca contra resposta absurda.
#define CAT_MAX 2000

typedef struct {
  char backdrop[512];
  char poster[512];
  char logo[512];      // vazio quando o titulo nao tem logo
  char titulo[160];
  char genero[160];    // "Programa de TV · Drama · Misterio"
  char meta[96];       // "2022 · 3 temporadas"
  char classificacao[8];
  char sinopse[900];
  // Elenco real: nome, papel e a foto (quando o TMDB tem). Sem isto a secao
  // "Elenco e equipe" fica com nomes inventados, e nomes inventados nao testam
  // o layout — os de verdade tem tamanhos que quebram a coluna.
  // `tmdb` e o id da PESSOA no TMDB, nao do titulo: e a chave para abrir a
  // filmografia dela (/person/<id>?append_to_response=combined_credits), que e
  // o que o web faz no `openCastDetail`. Sem ele o unico caminho seria procurar
  // por nome, que erra em homonimo e em nome com acento.
  struct { char nome[64]; char papel[64]; char foto[512]; long tmdb; } elenco[6];
  int nElenco;
  char direcao[128];
  // Nota da critica em porcentagem e o logo do servico onde o titulo esta. Sao
  // as duas coisas que a linha tecnica do app da Apple mostra alem do ano e da
  // duracao — sem elas a linha fica com metade da informacao.
  int  nota;              // 0 = desconhecida
  // Pais de producao, para a ultima linha de meta do detalhe (o web mostra
  // 'United States of America' ali). Vem do /meta, nao do catalogo.
  char pais[64];
  char provLogo[512];
  char provNome[64];
  // Onde assistir alem da assinatura: aluguel e compra na regiao BR do TMDB.
  // Vazios = o servico nao oferece o titulo por esse meio aqui. O detalhe
  // encolhe a secao de acordo — um card sem dado e pior que a ausencia dele.
  char alugLogo[512];
  char alugNome[64];
  char compLogo[512];
  char compNome[64];
  // Identificador do titulo no IMDb ("tt11280740") e o tipo que os addons usam
  // ("movie"/"series"). Vem de art/ids.txt, resolvido pelo Cinemeta — sem ele
  // nao ha como perguntar fontes a addon nenhum.
  // Quanto do titulo o dono ja assistiu, 0..100. Vem do app web (chave
  // watchProgressItems), quarta coluna de extra.txt. 0 = nao comecou.
  int  progresso;
  // Legenda do card em "Continue Assistindo". Serie mostra "T1, E8 · 16 min";
  // filme mostra so o tempo que falta. Temporada/episodio ficam em 0 no filme,
  // e e isso que separa os dois casos no desenho.
  int  temporada, episodio;
  int  restanteMin;
  char nomeEpisodio[120]; // titulo do episodio em andamento, nunca nome do arquivo
  // Temporadas que a serie tem, na ordem. Sai do campo `videos` do Cinemeta,
  // buscado quando o titulo abre. 0 = ainda nao se sabe (ou e filme), e as
  // abas caem no padrao de 3 que existia fixo.
  int  temporadas[12];
  int  nTemporadas;
  // Vem do Trakt: 1 se esta na watchlist do dono, 1 se esta na colecao dele.
  // Ficam no item e nao numa tabela a parte da biblioteca porque o catalogo e
  // reconstruido da rede — uma tabela por indice apontaria para outro titulo
  // depois da primeira atualizacao.
  int  naLista, naColecao;
  char imdb[16];
  char tipo[8];
  // Autoria do feed social, separada dos metadados do filme.
  char socialNome[96], socialSlug[128], socialAvatar[768], socialAcao[64];
  // Id do titulo no TMDB, quando a busca por imdb_id ja o resolveu (ver
  // fotosDoElenco em descoberta.c). Era descartado; e por ele que se chega a
  // COLECAO do filme, que o TMDB so expoe por id proprio.
  long tmdb;
  // QUANDO isto foi visto pela ultima vez, em ms desde a epoca. 0 = nao se sabe.
  //
  // Existe para ordenar a fileira "Continuar assistindo", que agora une DUAS
  // fontes (o /sync/playback do Trakt e o progresso da conta Nuvio, que e o que
  // chega do celular). Sem um instante que viaje COM o item nao ha como decidir
  // qual das duas versoes da mesma obra e a atual, e a fileira sairia na ordem
  // de quem respondeu primeiro.
  //
  // Precisa morar no CatItem, e nao num vetor paralelo, porque
  // trakt_enfeitar_lote COMPACTA o lote (tira o que o Cinemeta nao conhece):
  // um vetor de instantes indexado por posicao dessincroniza ali, em silencio.
  long long retomadoMs;
} CatItem;

// Um episodio de serie. Vem de art/episodios.txt, gerado a partir do campo
// `videos` do Cinemeta (/meta/series/<id>.json) — os mesmos episodios que os
// addons indexam, entao o que a tela lista e o que da para pedir fonte.
typedef struct {
  int  temporada, episodio;
  char nome[120];
  char duracao[16];    // "38 min"; vazio quando o Cinemeta nao informa
  // Data por EXTENSO, como o web: "27 de janeiro de 2023". Ele usa
  // toLocaleDateString com {month:"long", day:"numeric", year:"numeric"}
  // (metaDetailsScreen.js:1387) — "27/01/2023" era invencao do port. 16 bytes
  // nao cabiam: "15 de novembro de 2024" tem 22.
  //
  // Quem desenha encurta para so o ano quando `showFullReleaseDate` esta
  // desligado (ajustes_data_completa()); o ano sao os 4 ultimos caracteres.
  char data[40];
  char sinopse[420];
  char thumb[512];     // still do episodio; vazio cai na arte do titulo
} CatEp;

// Le <dir>/catalogo.txt. Devolve quantos itens carregou (0 = nenhum, e quem
// chama deve seguir com o que tiver).
int  cat_carregar(const char *dirArte);

// --- CACHE EM DISCO DO CATALOGO MONTADO PELA REDE ----------------------------
//
// Medido na TV: 14,5 s entre abrir o app e o catalogo da rede estar completo, e
// TODA abertura refazia os ~30 pedidos. O catalogo do PACOTE (catalogo.txt)
// cobria esse vao com 40 titulos estaticos que nao sao os do dono.
//
// Aqui o que a descoberta montou e gravado como esta na memoria e relido na
// proxima abertura, antes de qualquer rede. A rede continua rodando por cima e
// substitui quando chega — o cache nao e a verdade, e o que mostrar enquanto a
// verdade nao chega.
//
// Formato BINARIO e nao texto: CatItem e POD (so vetores de char e inteiros,
// nenhum ponteiro), entao gravar em bloco e correto e dispensa um serializador
// que teria de ser mantido em sincronia com a struct a cada campo novo. O
// cabecalho guarda `sizeof(CatItem)` e uma versao: se a struct mudar, o arquivo
// e RECUSADO em vez de lido torto. Ler lixo aqui seria pior que nao ter cache.
//
// ONDE O ARQUIVO MORA. `dirArte` continua no parametro por compatibilidade com
// os chamadores, mas e o ULTIMO recurso: as duas funcoes preferem dados_dir(),
// a pasta gravavel descoberta no arranque. No alvo Tizen `dirArte` e /app/art,
// que vem de --preload-file e portanto e MEMFS — RAM apagada a cada recarga —,
// e la o cache NUNCA sobreviveu a fechar o app: toda abertura refazia os ~30
// pedidos e os 14,5 s medidos acima. A escolha fica dentro de catalogo.c, e nao
// nos chamadores, porque quem le (home.c) e quem grava (descoberta.c) sao
// arquivos diferentes e tem de concordar. Ver a nota longa em caminhoCache.
int  cat_gravar_cache(const char *dirArte);
// Devolve 1 se carregou. Chamar DEPOIS de cat_carregar: ele substitui o
// catalogo do pacote quando o cache existe e e valido.
int  cat_ler_cache(const char *dirArte);

// APAGA O CACHE DO CATALOGO. 1 se havia arquivo e ele saiu.
//
// TEM DE SER CHAMADA NO LOGOUT, em sync_esquecer_usuario, junto das outras onze
// coisas que ja sao esquecidas ali. Sem isso, a primeira abertura depois de
// trocar de conta mostra a home da conta ANTERIOR — watchlist, continuar
// assistindo, feed de amigos com nome e avatar — ate a rede substituir.
//
// E o dano nao para no estetico: cada CatFileira grava `base[600]`, campo desse
// tamanho porque o Xperience embute um JWT no CAMINHO do addon (ver a nota do
// campo). Um cache que sobrevive ao logout e credencial do usuario anterior
// deixada em disco, do mesmo tipo que fez collections.json ser excluido do
// pacote em tools/arm.sh.
//
// O cabecalho tambem grava a identidade (o `sub` do JWT e o perfil ativo) e
// cat_ler_cache RECUSA E APAGA o que nao bater — mas isso e a rede de
// seguranca, nao a porta da frente: enquanto o arquivo estiver la, ele estara
// la. A verificacao cobre um caso que o logout nao ve, a troca de PERFIL dentro
// da mesma conta, que nao passa por sync_esquecer_usuario.
int  cat_apagar_cache(void);
// 1 enquanto o que esta na tela veio do CACHE, e nao da rede desta sessao.
//
// A descoberta publica cada fileira assim que ela chega, o que e certo numa
// tela vazia e ERRADO sobre o cache: a home iria de 16 fileiras para 1 e
// voltaria a crescer na frente do dono. Com o cache no ar, ela espera o
// catalogo completo. Sem cache, publica em partes como antes.
int  cat_do_cache(void);
void cat_cache_substituido(void);
int  cat_n(void);
const CatItem *cat_item(int i);

// Indice do titulo com este IMDb id, ou -1. O id do catalogo pode trazer
// episodio ("tt123:2:1"); a comparacao para no primeiro ':' dos dois lados.
//
// Existe para abrir um titulo a partir de um id que veio de FORA do catalogo —
// a filmografia de um ator e a aba "Mais como este" devolvem tt..., e sem esta
// busca nao haveria como saber se aquele titulo e um dos que ja temos meta.
// Onde progresso.txt e gravado. Passou a ser necessario com o login: o
// progresso e dado DO USUARIO e nao pode morar na pasta do pacote, que e a
// mesma para todo mundo que usar o aparelho. Chamar depois de cat_carregar.
void cat_dir_gravacao(const char *dir);

int cat_indice_por_imdb(const char *imdb);

// Acrescenta um titulo ao FIM e devolve o indice, ou -1. Para o titulo que veio
// de fora do catalogo (filmografia de ator, "Mais como este"). Ver a nota sobre
// a troca de bloco em catalogo.c.
int cat_acrescentar(const CatItem *item);

// Acrescenta `qtd` itens numa UNICA troca de bloco e escreve os indices em
// `saidaIdx` (pode ser NULL). Devolve quantos entraram.
//
// Use este, e nao cat_acrescentar em laco, sempre que houver mais de um: aquele
// copia o catalogo inteiro por chamada, e a busca chegava a mover dezenas de MB
// no fio de desenho a cada tecla.
int cat_acrescentar_lote(const CatItem *v, int qtd, int *saidaIdx);

// Atualiza o espelho local de "esta na watchlist". A verdade e o Trakt, mas
// esperar o proximo ciclo de descoberta para o botao mudar de cara faria o
// toque parecer sem efeito.
void cat_definir_na_lista(int i, int naLista);

// Grava onde o dono parou NESTE app: escreve em progresso.c (pendente, com a
// chave do web) e atualiza o item. E o caminho do player.
// Apaga a posicao de retomada de UM item, so no catalogo em memoria. Quem
// apaga o registro persistido e prog_remover; esta funcao existe para o card
// sair da fileira "Continuar assistindo" no mesmo quadro, sem esperar a
// proxima remontagem do catalogo.
// Tira um item da janela da fileira que o contem, sem mexer nas outras (as
// janelas sao disjuntas). Devolve 1 se achou. Ver a nota em catalogo.c: zerar
// o progresso apaga a legenda mas deixa o card na fileira, e era isso que fazia
// a remocao de "Continuar assistindo" so aparecer na proxima abertura (#22).
int cat_tirar_item_da_fileira(int indice);
void cat_zerar_progresso(int indice);

void cat_salvar_progresso(int indice, double posSeg, double durSeg);
void cat_salvar_progresso_ep(int indice, double posSeg, double durSeg, int temporada, int episodio);

// So a MEMORIA do item (barra, minutos restantes, episodio em andamento), sem
// tocar em arquivo. E o que o sync usa para o que veio da conta — a decisao de
// gravar ou nao ja foi tomada em progresso.c.
void cat_aplicar_progresso(int indice, double posSeg, double durSeg, int temporada, int episodio);

// O item passa a apontar para outro episodio, sem mexer em progresso. Pos-play
// usa ao pular para o proximo: e dele que sai o rotulo do player.
void cat_apontar_episodio(int indice, int temporada, int episodio);

// Episodios do titulo `indiceItem`. Filme devolve 0 — e o que a tela usa para
// decidir se mostra a secao de episodios.
// Substitui o catalogo inteiro pelo que veio da rede. Os caminhos de arte
// passam a ser URLs — o tex_cache baixa e guarda em disco sozinho.
void cat_definir(const CatItem *lista, int n);

// --- FILEIRAS DA HOME --------------------------------------------------------
// O app web nao tem fileira fixa. Cada fileira e UM CATALOGO de UM addon, e a
// lista sai de `homeCatalogPrefs` (por perfil), aplicada em
// `sortAndFilterRowsInternal` (js/ui/screens/home/homeScreen.js:9856):
//
//   1. junta catalogos e colecoes num mapa indexado por `homeCatalogKey`
//   2. `ensureOrderKeysWithPrefs` devolve a ordem salva com as chaves NOVAS
//      acrescentadas no FIM — catalogo que apareceu depois entra por ultimo
//   3. tira as desativadas, conferindo DUAS chaves: `homeCatalogDisableKey`
//      (<baseUrl>_<tipo>_<catalogoId>_<nome>) e `homeCatalogKey`
//   4. aplica `customTitles[homeCatalogKey]` sobre o nome do catalogo
//   5. colecoes com `pinToTop` vao na frente e nunca sao cortadas
//   6. corta o total em `getHomeRowLimit()`
//
// O teto para NOS e 16: `HOME_MAX_ROWS_LEGACY_TV` em homeConstants.js, o ramo
// que `isLegacyTvRuntime()` escolhe — e esta TV e exatamente esse caso. O 40 do
// `HOME_MAX_ROWS_DEFAULT` e do navegador de mesa.
#define CAT_FIL_MAX 16

typedef struct {
  char chave[192];   // homeCatalogKey: <addonId>_<tipo>_<catalogoId>
  char titulo[96];   // ja formatado, com o sufixo de tipo
  char tipo[8];      // "movie" | "series"
  // DE ONDE A FILEIRA VEIO. A chave acima identifica o catalogo mas nao serve
  // para CHAMAR de novo: ela carrega o id do ADDON, nao o endereco dele.
  // Sem estes dois nao ha como pedir a continuacao da lista, que e o que a tela
  // "Ver tudo" faz — ela chama o mesmo catalogo com `skip`.
  // 600 e nao 300: o Xperience embute um JWT no CAMINHO e a base dele tem 367
  // caracteres. Com 300 ela era truncada em silencio, a URL montada aqui virava
  // outra coisa e o catalogo respondia sem `metas` — a tela "Ver tudo" abria
  // vazia sem nenhum erro. addons.c ja usa 600 pelo mesmo motivo.
  char base[600];
  char catId[96];
  // Janela no vetor de itens. As fileiras NAO tem vetor proprio: apontam para
  // o catalogo unico, que e o que a biblioteca e a busca varrem. Duplicar os
  // itens por fileira custaria ~3,5 KB por titulo repetido.
  int  ini, n;
} CatFileira;

int cat_n_fileiras(void);
const CatFileira *cat_fileira(int r);   // NULL fora da faixa

// Troca itens E fileiras de uma vez. Tem de ser uma chamada so: com duas, o fio
// do desenho pega um quadro com as fileiras novas apontando para os itens
// velhos, e a janela (ini,n) cai fora do vetor.
// TROCA SO A LISTA DE FILEIRAS, mantendo os itens onde estao.
//
// As fileiras sao janelas (ini,n) no vetor de itens, entao reordena-las, filtrar
// algumas ou mudar o limite NAO exige tocar nos itens — e portanto nao exige
// buscar nada de novo na rede. Existe para separar "rebuscar" de "remontar":
// mudanca de ordem, de colecao ou do limite e so remontagem, e disparar um ciclo
// inteiro de descoberta por causa dela era o que fazia a home carregar um
// catalogo, trocar por outro e so entao assentar na ordem certa.
//
// Mesma disciplina de cat_definir_tudo para nao precisar de trava: zera a
// contagem primeiro (o desenho ve zero fileiras por um quadro), preenche, e so
// entao sobe a contagem. Os itens nao se movem, entao as janelas continuam
// validas o tempo todo.
void cat_republicar_fileiras(const CatFileira *fils, int nNovas);

void cat_definir_tudo(const CatItem *lista, int qtd,
                      const CatFileira *fils, int nFils);


// Substitui os episodios de UM titulo. Chamado quando o detalhe abre.
void cat_definir_episodios(int indiceItem, const CatEp *lista, int n);

// Substitui UM item, preservando o resto. Usado quando o detalhe abre e traz
// elenco, direcao e temporadas que o catalogo da fileira nao tinha.
void cat_atualizar_item(int indice, const CatItem *novo);

// Titulos parecidos com o de `indice`: mesmo tipo (filme/serie) e pelo menos um
// genero em comum, os de nota mais alta primeiro. Devolve quantos escreveu.
//
// Nao ha endpoint de "similares" no protocolo dos addons — o Cinemeta nao tem e
// o Xperience so oferece "More Like X" para os titulos recentes do dono, nao
// para um qualquer. Cruzar genero dentro do catalogo que ja esta carregado
// responde na hora, sem rede, e acerta o suficiente para a fileira valer.
int           cat_similares(int indice, int *saida, int max);

// Quantas vezes o catalogo INTEIRO foi trocado. Muda => todo indice guardado
// fora daqui deixou de valer, e as faixas de episodio foram zeradas.
unsigned      cat_revisao(void);
int           cat_n_episodios(int indiceItem);
const CatEp  *cat_episodio(int indiceItem, int i);   // indice circular; NULL se o catalogo esta vazio

#endif
