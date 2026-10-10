// Catalogo de titulos vindo de um arquivo, no lugar das listas fixas no codigo.
//
// Existe porque testar layout com nomes inventados esconde problemas reais: os
// titulos de verdade tem tamanhos muito diferentes ("CODA" contra "Assassinos
// da Lua das Flores"), acentos, e sinopses que nao cabem em tres linhas. Cada
// item tambem carrega o LOGO do titulo, que e o que o app da Apple desenha no
// lugar do nome em texto.
#ifndef NV_CATALOGO_H
#define NV_CATALOGO_H
#include "addonurl.h"

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
#define CAT_TEMP_MAX 64
// Elenco guardado por titulo. O Cinemeta traz 3-5 nomes no `cast` do meta e era
// isso que a tela mostrava (issue #94); quem completa a fileira e o TMDB, que
// fotosDoElenco acrescenta depois de enriquecer os que ja vieram. A pagina de
// detalhe desenha ate NV_DETF_EL_MAX (18), entao 12 ainda nao enche a fileira
// inteira — e o teto que cabe sem inchar o CatItem que o cache grava em disco.
#define CAT_ELENCO_MAX 12

typedef struct {
  char backdrop[512];
  // Variantes de arte preservadas para o hero. `backdrop` continua sendo a
  // arte efetiva do catalogo, enquanto estas tres guardam as origens quando
  // elas chegam separadas — sem obrigar a TV a consultar a rede ao trocar a
  // fonte no Ajustes.
  char backdropCatalogo[512]; // background vindo do addon/Cinemeta
  char backdropTmdb[512];     // backdrop vindo do TMDB
  char backdropTrakt[512];    // fanart vindo do Trakt
  // 1024 e nao 512 (#200): a URL de cartaz que o AIOMetadata monta para o
  // PostersPlus passa de 700 bytes e o corte calado levava o idioma embora.
  // Mesmo teto do cache de textura (NV_TEX_URL_MAX, tex_cache.h).
  char poster[1024];
  char logo[512];      // vazio quando o titulo nao tem logo
  // Language evidence belongs to this exact logo, never to another selection.
  char logoIdioma[8];
  char logoIdiomaUrl[512];
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
  struct { char nome[64]; char papel[64]; char foto[512]; long tmdb; } elenco[CAT_ELENCO_MAX];
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
  //
  // 64 E NAO 12 (#79, Owlphibia: South Park e Os Simpsons paravam na T12).
  // 12 era o que cabia na fileira de abas sem rolar; South Park tem 27
  // temporadas, Os Simpsons 37, SNL 50. A fileira rola; o teto so precisa
  // caber na struct. Mudar o tamanho invalida o cache em disco sozinho (ver
  // a nota sobre sizeof(CatItem) abaixo).
  int  temporadas[CAT_TEMP_MAX];
  int  nTemporadas;
  // Vem do Trakt: 1 se esta na watchlist do dono, 1 se esta na colecao dele.
  // Ficam no item e nao numa tabela a parte da biblioteca porque o catalogo e
  // reconstruido da rede — uma tabela por indice apontaria para outro titulo
  // depois da primeira atualizacao.
  int  naLista, naColecao;
  // 64 E NAO 16. O campo nasceu para "tt11280740", e 16 bastava. O id de um
  // addon de canal e "cs:channel:globo-tv-integracao-ituiutaba" — 16 bytes
  // cortavam TODO canal da Globo em "cs:channel:glob": todos viravam o mesmo
  // item (clicar num abria outro), e /stream/channel/cs:channel:glob.json
  // voltava vazio ("source error"). Medido no Mac com a conta do dono, #37.
  // O cache em disco carrega sizeof(CatItem) no cabecalho e se invalida
  // sozinho com esta mudanca.
  char imdb[64];
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
  // trakt_enfeitar_lote COMPACTA o lote (tira quem ficou sem poster ou
  // "a seguir" sem confirmacao): um vetor de instantes indexado por posicao
  // dessincroniza ali, em silencio.
  long long retomadoMs;
  // DE QUAL ADDON ESTE ITEM VEIO (o catalogo ou a busca que o trouxe): o "id" do
  // manifesto, ou "#<hash da base>" enquanto o manifesto nao foi lido. Nunca a
  // URL (ela carrega credencial e este struct vai para o cache em disco).
  //
  // E o que deixa o detalhe perguntar a ficha (/meta) primeiro a QUEM PUBLICOU o
  // titulo: "kitsu:41370" so quem o publicou sabe abrir, e o Cinemeta nunca o
  // conheceu. Vazio = origem desconhecida (Trakt, Salvos, progresso, pacote).
  // Mudar o tamanho invalida o cache em disco sozinho (ver sizeof(CatItem)).
  char origem[96];
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
  // Nota do episodio x10 (83 = 8,3), 0 = desconhecida. O Cinemeta nao tem voto
  // por episodio; quem preenche e o TMDB (/tv/<id>/season/<n>), uma viagem por
  // temporada, no fio de desc_episodios (issue #87). O campo NAO invalida
  // cache nenhum: o unico dump binario e o do CatItem (catalogo-rede.bin) e
  // episodios.txt e texto, campo a campo — ambos leem o que sabem ler.
  int  nota;
  // O ID DO VIDEO como o /meta o publicou ("kitsu:41370:5", "tt123:1:2"). E com
  // ele que se pede fonte (/stream/series/<id>.json): o addon de anime tem id
  // proprio por episodio, e montar "<titulo>:<T>:<E>" na mao so acerta no IMDb.
  // Vazio = o meta nao trouxe; ver cat_id_stream.
  char vid[64];
  // O MESMO EPISODIO NO TMDB (2.0.3): id da serie e o par temporada/episodio
  // que casou em /tv/<id>/season/<n> (desc_tmdb_notas_temporada_ex). 0 = o
  // TMDB nao confirmou. Com ele o TheIntroDB e consultado por tmdb_id, sem o
  // remapeamento IMDb->TMDB que a API faz (intro.h). Nao vai para o disco.
  long tmdbSerie;
  int  tmdbT, tmdbE;
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
int  cat_gravar_cache_se_identidade(const char *dirArte, const char *donoEsperado,
                                    int perfilEsperado);
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
// E o dano nao para no estetico: cada CatFileira grava `base` (addonurl.h), campo desse
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
// O ponteiro devolvido (e os campos dele: backdrop, poster, logo) vale ate o
// FIM DO QUADRO em que foi pedido, mesmo que a descoberta troque o catalogo
// quantas vezes for nesse meio tempo. Nao guarde de um quadro para o outro:
// guarde o indice, ou copie. Ver cat_quadro.
const CatItem *cat_item(int i);
// VIRADA DE QUADRO, chamada pelo fio de desenho no comeco de cada quadro
// (main.c), num ponto onde nenhum ponteiro de cat_item() esta na mao.
//
// E o que decide quando um bloco trocado fora pode ser liberado. Antes ele
// morria na troca SEGUINTE — e duas trocas dentro de um quadro (a publicacao
// por fileira, ou a montagem junto do fio de "Continuar assistindo") liberavam
// o bloco em que o desenho ainda lia: `item->backdrop` chegava ao cache de
// textura como lixo binario ("[tex] decode falhou (Couldn't open ���̑C)").
// Agora o bloco so e liberado depois que o quadro seguinte a troca COMECOU.
void cat_quadro(void);
// Quantos blocos trocados fora ainda esperam a virada de quadro. Para teste.
int  cat_blocos_aposentados(void);

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
// Copia independente por id base + tipo, sob a trava dos publicadores.
// Para fios que precisam reaproveitar metadados sem guardar cat_item().
// Prefere a copia com poster; 0 quando o titulo ainda nao esta no catalogo.
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *saida);
// Como cat_indice_por_imdb, mas fica em `preferido` enquanto ele for o mesmo
// titulo e prefere uma copia COM episodios (#151; ver catalogo.c).
int cat_indice_titulo(const char *imdb, int preferido);
// O indice de quem GUARDOU `indice` junto com o id do titulo (#190): o
// proprio `indice` enquanto ele ainda for aquele titulo (id identico, custo de
// um strcmp), senao cat_indice_titulo. -1 se o titulo nao esta mais no
// catalogo. Sem id, devolve `indice` como antes.
int cat_indice_vivo(int indice, const char *imdb);

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
// A watchlist/colecao do Trakt por cima do bloco da tela, SEM trocar fileira:
// titulo que ja esta no catalogo so ganha a marca (naLista/naColecao, em todas
// as copias), o resto entra no fim, como cat_acrescentar_lote. Nada sai — a
// publicacao completa da descoberta e quem poda o que deixou a lista. Devolve
// quantos entraram.
int cat_mesclar_listas(const CatItem *v, int qtd);

// Atualiza o espelho local de "esta na watchlist". A verdade e o Trakt, mas
// esperar o proximo ciclo de descoberta para o botao mudar de cara faria o
// toque parecer sem efeito.
void cat_definir_na_lista(int i, int naLista);
// Por TITULO, em todas as copias do catalogo (ver catalogo.c).
int  cat_definir_na_lista_imdb(const char *imdb, int naLista);
int  cat_imdb_na_lista(const char *imdb);   // alguma copia do titulo esta salva

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
// Tira de "Continuar assistindo" todos os cards da OBRA de `imdb` (composto ou
// nao), por identidade e sob a trava dos publicadores — o indice guardado pela
// modal pode ter mudado de dono com uma refacao em voo. Sobe cat_revisao, entao
// a home remonta no mesmo quadro. Devolve quantos cards sairam.
int cat_tirar_continuar(const char *imdb);
void cat_zerar_progresso(int indice);
// TITULO INTEIRO VISTO (#212): o selo do cartaz e o olho do detalhe. Historico
// conhecido (Trakt, conta, acao da pessoa) manda; sem ele, progresso >= 90 so
// em filme. O(1), pode ser chamada por cartaz em todo quadro.
int cat_visto(const CatItem *c);
// A fronteira efetiva de conta/perfil troca o mapa em O(HIST_BALDES), uma
// vez; repetir a mesma identidade conserva provas locais e de outras fontes.
void cat_historico_contexto(const char *usuario, int perfil);
unsigned long long cat_historico_geracao(void);
int cat_historico_estado_id(const char *imdb, const char *tipo);
int cat_historico_estado_item(int indice);
void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);
// Worker captura geracao ANTES da rede. A resposta so pertence ao mapa se a
// identidade ainda for a mesma; verificacao e escrita compartilham a trava.
int cat_historico_definir_se_geracao(const char *imdb, const char *tipo,
                                     int visto, unsigned long long geracao);

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
// O teto para NOS era 16: `HOME_MAX_ROWS_LEGACY_TV` em homeConstants.js, o ramo
// que `isLegacyTvRuntime()` escolhe — e esta TV e exatamente esse caso. O 40 do
// `HOME_MAX_ROWS_DEFAULT` e do navegador de mesa.
//
// 40 desde a "Fileiras da home" ate 40 (pedido do dono, com aviso de memoria):
// o mesmo numero do navegador de mesa, e o maior que a estrutura aguenta sem
// mexer em mais nada. CONTA, nao medicao em TV: o custo de uma fileira e o dos
// seus itens, e CatItem pesa 15,6 KB; 40 fileiras x 24 itens = 960 titulos
// (~15 MB de catalogo) contra CAT_MAX = 2000, entao o vetor de itens continua
// sendo o teto de verdade e nao estoura. Cada CatFileira pesa ~2,5 KB (era ~1 KB
// ate a base passar a NV_ADDON_URL_MAX, #201): os vetores
// de fileira (40 x 2,5 KB = 100 KB) sao static onde eram de pilha — ver
// cat_ler_cache, cat_trocar_continuar e montar().
#define CAT_FIL_MAX 40

typedef struct {
  char chave[192];   // homeCatalogKey: <addonId>_<tipo>_<catalogoId>
  char titulo[96];   // ja formatado, com o sufixo de tipo
  char tipo[16];     // "movie" | "series" | tipos de canal ("channels" tem 9)
  // DE ONDE A FILEIRA VEIO. A chave acima identifica o catalogo mas nao serve
  // para CHAMAR de novo: ela carrega o id do ADDON, nao o endereco dele.
  // Sem estes dois nao ha como pedir a continuacao da lista, que e o que a tela
  // "Ver tudo" faz — ela chama o mesmo catalogo com `skip`.
  // 600 e nao 300: o Xperience embute um JWT no CAMINHO e a base dele tem 367
  // caracteres. Com 300 ela era truncada em silencio, a URL montada aqui virava
  // outra coisa e o catalogo respondia sem `metas` — a tela "Ver tudo" abria
  // vazia sem nenhum erro. addons.c ja usa 600 pelo mesmo motivo.
  // 600 TAMBEM NAO BASTAVA (#201): a URL do Comet tem 870. O tamanho agora e o
  // de addonurl.h, o mesmo de quem guarda a lista. O campo vai CRU para o
  // catalogo-rede.bin; o cabecalho do arquivo leva sizeof(CatFileira)
  // (tamFileira), entao o cache de uma build de 600 e recusado e apagado na
  // primeira leitura, e a home daquela abertura vem da rede.
  char base[NV_ADDON_URL_MAX];
  char catId[96];
  // Janela no vetor de itens. As fileiras NAO tem vetor proprio: apontam para
  // o catalogo unico, que e o que a biblioteca e a busca varrem. Duplicar os
  // itens por fileira custaria ~3,5 KB por titulo repetido.
  int  ini, n;
  // 1 = resposta valida explicitamente vazia. A linha permanece na
  // estrutura para que uma resposta parcial nao desloque as seguintes.
  int estado;
  // Social-only account generation captured BEFORE fetching its source data.
  // Metadata/progress revisions never make an old account row current.
  unsigned socialGeracao;
} CatFileira;

int cat_n_fileiras(void);
const CatFileira *cat_fileira(int r);   // NULL fora da faixa
// Empty or only Continue Watching/social: startup may publish catalogue rows
// as they arrive. Any other row, saved-list marker or unassigned item is warm.
// One coherent snapshot under the publication mutex; no transient n==0 read.
int cat_home_apenas_fixas(void);
int cat_copiar_fileira(const char *chave, CatItem *itens, int max,
                       CatFileira *meta);

// Refaz SO a fileira "continue_watching" (issue #38): os itens novos tomam o
// lugar da janela dela no vetor unico, as demais fileiras deslizam no `ini` e
// a revisao sobe para a home remontar. Chamada pelo fio dedicado da
// descoberta — montarContinuar faz rede e nao pode rodar no desenho.
void cat_trocar_continuar(const CatItem *lista, int qtd);

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

// Assinatura do que a home desenha (fileiras + identidade dos itens). A
// descoberta compara a do candidato com a da tela e SO publica se mudou.
unsigned long cat_assinatura(void);
unsigned long cat_assinatura_de(const CatItem *lista, int qtd,
                                const CatFileira *fl, int nf);
void cat_definir_tudo(const CatItem *lista, int qtd,
                      const CatFileira *fils, int nFils);


// Substitui os episodios de UM titulo. Chamado quando o detalhe abre.
void cat_definir_episodios(int indiceItem, const CatEp *lista, int n);

// Substitui UM item, preservando o resto. Usado quando o detalhe abre e traz
// elenco, direcao e temporadas que o catalogo da fileira nao tinha.
void cat_atualizar_item(int indice, const CatItem *novo);
// O mesmo, mas as abas de temporada (temporadas/nTemporadas) ficam as que o item
// JA tem, lidas sob a trava. Para a cauda de enriquecimento (elenco, arte) do
// detalhe, que roda depois de soltar o fio de episodios: um fio mais novo do
// mesmo titulo pode ter publicado outra lista com outras abas (#372).
void cat_atualizar_item_sem_abas(int indice, const CatItem *novo);
// Copia do item `indice` sob a trava dos publicadores; 0 se o indice nao existe.
// Para fios fora do desenho (o ponteiro de cat_item() so vale no quadro).
int  cat_copiar_item(int indice, CatItem *saida);
// So a sinopse (e o titulo, se vazio) de `indice`, se ele ainda e `imdb` e
// ainda nao tem sinopse. Nao toca em mais nada. 1 = escreveu.
int  cat_completar_sinopse(int indice, const char *imdb, const char *sinopse,
                           const char *titulo);
// Texto localizado de `indice` (titulo, sinopse, logo, fundo), se ele ainda e
// `imdb`. Campo vazio nao apaga; nada mais do item e tocado. 1 = mudou.
int  cat_aplicar_localizado(int indice, const char *imdb, const char *titulo,
                            const char *sinopse, const char *logo, const char *fundo);

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
// Sobe sempre que alguma faixa de episodio e apagada (troca de bloco OU a
// volta do vetor comum de episodios, que NAO mexe em cat_revisao). Quem mostra
// a lista de uma serie repede quando ela muda e a lista dele esta vazia.
unsigned      cat_geracao_episodios(void);
// Relogio monotonico em ms, para os marcadores [perf] da publicacao.
double        cat_relogio_ms(void);
// Sobe a cada mudanca em QUALQUER item (marca de lista, progresso, item novo,
// substituido ou removido), alem de toda troca do bloco. Barata de ler por
// quadro; quem deriva uma lista do catalogo reconstroi so quando ela muda.
unsigned      cat_revisao_itens(void);
int           cat_n_episodios(int indiceItem);
const CatEp  *cat_episodio(int indiceItem, int i);   // indice circular; NULL se o catalogo esta vazio
// O id que se pede aos addons de FONTE para o episodio (t,e) do item. Serie do
// IMDb: "tt:T:E" (o id do video so vale se tambem for "tt"); serie de outro
// espaco de ids ("kitsu:41370"): o id do video que o /meta trouxe, e na falta
// dele "<id>:<E>" (a convencao do Kitsu/MAL). t<=0 ou e<=0 = sem episodio: devolve o id
// do titulo. 1 se escreveu algo.
int           cat_id_stream(int indiceItem, int t, int e, char *dst, unsigned tam);

#endif
