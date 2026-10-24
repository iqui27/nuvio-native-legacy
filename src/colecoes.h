#ifndef NV_COLECOES_H
#define NV_COLECOES_H
/* TETOS (#255). Uma conta do Xperience chega a ~300 pastas e ~600 fontes em
   7 colecoes; o teto antigo de 256 pastas cortava a sexta colecao no meio e
   jogava a setima fora, e as fontes delas viravam fileiras soltas na Home.

   A memoria agora acompanha a conta: as pastas moram em blocos de 64 alocados
   quando a montagem chega neles (e nunca devolvidos, para o ponteiro de
   col_folder continuar valendo), e as fontes num bloco so do tamanho do que a
   conta mandou. Antes eram 256 pastas x 32 fontes embutidas = 13,4 MB fixos,
   fosse a conta de 3 pastas ou de 256.

   O teto que sobra e de seguranca, nao de conta real. Quando ele e atingido a
   colecao INTEIRA fica de fora (nunca cortada no meio) e o log diz qual. */
#define COL_MAX 2048
#define COL_SOURCE_MAX 32
#define COL_FONTES_MAX 4096
typedef struct {
  char title[128], base[600], type[8], catId[96], genre[96]; char addonId[96];
  // PROVEDOR NAO-ADDON (issue #44). Vazio = catalogo de addon, identificado por
  // base/type/catId como sempre foi. O editor de colecoes do site tambem grava
  // fontes "tmdb" e "trakt", que nao tem catalogo de addon equivalente: quem
  // resolve e o vertudo, perguntando direto ao TMDB ou ao Trakt — ver
  // desc_vertudo_fonte em descoberta.c.
  char prov[8];
  char tmdbTipo[16];   // LIST|COLLECTION|PERSON|DIRECTOR|COMPANY|NETWORK; vazio = discover
  long tmdbId;         // id da lista/colecao/pessoa/empresa/rede; 0 = discover puro
  char midia[8];       // "MOVIE" | "TV" — escolhe o endpoint e o tipo do item
  char ordenar[48];    // sort_by do TMDB / sort_by da lista Trakt
  char ordem[8];       // sort_how do Trakt: "asc" | "desc"
  long traktLista;     // id da lista no Trakt
  char filtros[1024];  // Web snapshots include every null default (~500 bytes).
} ColSource;
typedef struct {
  char id[96], title[128], group[64], cover[512], hero[512], logo[512];
  /* backdropImageUrl da COLECAO (o grupo), a arte que vale para toda pasta que
     nao trouxe a sua. `hero` guarda so o heroBackdropUrl da PASTA. A escolha
     entre eles mora em col_banner/col_capa, na ordem do app web. */
  char groupBackdrop[512];
  char groupId[64];   /* id da colecao no web; a chave de ordem da conta e collection_<groupId> */
  char frameDir[600];
  char detailHero[512];
  /* GIF de foco que a CONTA manda (focusGifUrl). O PACOTE nao guarda URL:
     tools/import-collections.mjs ja converte o GIF em 001.jpg..090.jpg na
     importacao, e o que sobra dele aqui e frames+frameDir. Vazio quando a
     conta nao mandou o campo ou mandou focusGifEnabled:false. Ver src/gif.h
     para onde isso anima (Tizen) e onde nao anima (webOS). */
  char focusGif[512];
  int editorial; /* 1: legacy vector export; 2: approved cinematic image pair */
  int local;     /* 1: veio do collections.json do pacote (arte e ajustes curados aqui) */
  /* 1: pasta ACRESCENTADA PELO APP, nao vinda da conta nem do pacote. Ver
     col_extra_definir: e a marca que permite reinjeta-la depois de cada
     reconstrucao sem duplicar. */
  int extra;
  int frames, hideTitle, nSources;
  /* FORMA DO CARTAO da pasta (COL_FORMA_*): o `tileShape` que o editor de
     colecoes do app web grava por pasta. Ver col_forma_texto. */
  int forma;
  /* AS FONTES NAO MORAM MAIS NA PASTA (#255): eram 32 x 1512 bytes embutidos
     em cada uma, para uma media de 2 fontes por pasta. `sources` aponta para o
     bloco de fontes do conjunto (colecoes.c) e e valido para ler ate
     COL_SOURCE_MAX posicoes — o bloco termina com essa folga zerada. Quem
     monta uma pasta fora de colecoes.c (extras, a pasta sintetica do detalhe)
     aponta para um vetor seu; col_extra_definir copia as fontes. */
  ColSource *sources;
} ColFolder;
/* As tres formas do app web (collectionsStore.js, normalizePosterShape:
   POSTER, LANDSCAPE/WIDE, SQUARE). PAISAGEM e o zero de proposito: e como o
   nativo sempre desenhou, e o que fica para a pasta do pacote (collections.json
   nao traz o campo) e para toda pasta montada com memset. */
enum { COL_FORMA_PAISAGEM = 0, COL_FORMA_QUADRADO, COL_FORMA_POSTER, COL_FORMA_N };
/* Texto do JSON -> COL_FORMA_*, na regra do web: "POSTER" e pôster,
   "LANDSCAPE" ou "WIDE" e paisagem, e QUALQUER OUTRA COISA (inclusive ausente)
   e quadrado — e o que o web desenha para uma pasta da conta sem o campo. */
int col_forma_texto(const char *s);
/* A forma da FILEIRA de um grupo. O web desenha cartao por cartao; a fileira
   nativa tem um passo so, entao vale a forma da maioria das pastas do grupo
   (empate: a da primeira). COL_FORMA_PAISAGEM quando o grupo nao existe. */
int col_grupo_forma(const char *nome);
int col_carregar(const char *dir);
/* Pastas que o PROPRIO APP acrescenta (hoje: as listas do Trakt que a
   Biblioteca levou para a Home — ver src/listas.c).

   POR QUE PRECISA EXISTIR. col_definir_json RECONSTROI folders[] do zero a cada
   pull da conta. Uma pasta acrescentada por fora some no primeiro sync, e o
   sintoma seria a fileira da lista fixada desaparecendo da home sozinha alguns
   segundos depois do arranque — defeito mudo e intermitente, do mesmo tipo do
   #18. Guardadas aqui, elas sao REINJETADAS depois de cada reconstrucao.

   Substitui o conjunto inteiro (n=0 limpa). Devolve quantas ficaram. */
#define COL_EXTRA_MAX 24
int col_extra_definir(const ColFolder *v, int n);
// Colecoes da CONTA (sync_pull_collections), no shape do collectionsStore.js do
// web: collections[{id,title,backdropImageUrl,folders[{id,title,coverImageUrl,
// heroBackdropUrl,titleLogoUrl,hideTitle,sources[{provider,addonBaseUrl,type,
// catalogId,title,genre}]}]}]. Aceita o array, o objeto {collections}, a linha
// da RPC ({collections_json}) e collections_json como STRING escapada. An
// explicit empty snapshot clears account/package folders, retaining extras.
// Missing/null/truncated replies retain the current snapshot. Returns folders;
// callers use col_revisao(), including deletions and unchanged nonempty pulls.
int col_definir_json(const char *json);
// Retira referencias de addon comprovadamente desinstalado; pasta sem fonte
// sai. Fontes TMDB/Trakt e as demais de uma pasta mista permanecem.
int col_podar_fontes_addon(int (*ausente)(const char *id, const char *base));
// Pure validation, safe on the sync worker before caching an account reply.
int col_resposta_valida(const char *json);
// TROCA DE PERFIL: tira da tela as colecoes do perfil anterior (as da conta e
// as do pacote). As do perfil novo entram quando o sync as trouxer; se ele nao
// tiver nenhuma, a home fica sem colecoes. As extras (listas fixadas) ficam.
void col_esquecer_perfil(void);
/* "Arte das pastas da conta" (Ajustes, desligado de fabrica): 1 = a pasta da
   conta que casa com uma do pacote usa a capa/fundo/logo da conta onde a conta
   os tem (a capa leva o GIF da conta; o fundo desliga o modo editorial). 0 = a
   arte curada do pacote, como sempre. Trocar refaz o casamento com a ultima
   resposta da conta, na hora (a revisao muda e a home remonta). */
void col_arte_conta(int sim);
// Chave de fileira de um grupo: collection_<id da colecao> quando a colecao
// tem id (web e catordem usam o id), senao collection_<titulo do grupo>.
void col_chave_grupo(const char *group, char *dst, unsigned n);
int col_n(void);
// Muda sempre que o conjunto de pastas muda. Quem decide remontar a tela deve
// olhar ISTO e nao col_n(): trocar N pastas por outras N mantem o numero.
unsigned col_revisao(void);
// A complete account snapshot was accepted for this profile, even when empty.
int col_tem_conta(void);
const ColFolder *col_folder(int i);
// IMPRESSAO DA ESTRUTURA das colecoes para o snapshot da Home (homeestado.c):
// grupo, id e fontes de cada pasta, SEM arte (URL de arte muda sem criar
// fileira nova). Calculada inteira sob a trava das colecoes (#203): pode ser
// chamada de qualquer fio. Antes homeestado lia col_folder(i)->sources[s] no
// fio da montagem, sem trava, enquanto o sync no laco principal liberava o
// bloco de fontes (fecharMontagem / col_esquecer_perfil) — leitura de memoria
// ja liberada. Mesmo valor que o hashColecoes antigo de homeestado.c.
unsigned col_hash_estrutura(void);
// Identity-based grouping: two collections can have the same display title.
void col_chave_pasta(const ColFolder *pasta, char *dst, unsigned n);
int col_grupo_chave(const char *chave, int *indices, int max);
int col_grupo_forma_chave(const char *chave);
int col_grupo(const char *nome, int *indices, int max);
// A ORDEM DAS PASTAS da fileira de um grupo, pela escolha da fileira
// (fil_ordem, fileiras.h): recebe os indices como col_grupo_chave os devolve e
// os ordena NO LUGAR. FIL_ORD_LISTA nao mexe em nada. O ANO e o do TITULO da
// pasta (nv_ano_meta) — a pasta nao tem campo de data; sem ano vai para o fim,
// na ordem que ja estava. A semente fixa o embaralhado: a montagem da home
// passa a dela e o teste a sua, para o aleatorio ser estavel de um quadro para
// o outro e reproduzivel.
void col_ordenar_pastas(int *indices, int n, int ordem, unsigned semente);
// addonId dominante do grupo (1) ou "" quando as fontes sao de addons
// diferentes / nao sao de addon (0). Ver a definicao.
int col_grupo_addon(const char *nome, char *dst, unsigned n);
const ColFolder *col_por_catalogo(const char *base, const char *type, const char *id);
// Fontes de colecao ainda sem base resolvida. Ver a nota em colecoes.c: zero
// significa que toda colecao ja pode engolir o catalogo dela.
int col_fontes_sem_base(void);
// Despeja ate `max` fontes com a base REDIGIDA, para comparar os dois lados.
void col_despejar_fontes(int max);
// Ate onde a fileira (base,type,id) chegou ao procurar colecao: 0 nenhuma base
// igual, 1 so a base, 2 base+type, 3 os tres (e entao o grupo e que esta
// oculto). Preenche `grupo` com o nome do grupo do melhor casamento.
int col_diagnostico(const char *base, const char *type, const char *id,
                    char *grupo, unsigned n);
// A arte que a PASTA mostra, na ordem do app web (homeScreen.js,
// normalizeCollectionFolderItem): o BANNER (fundo do destaque e da tela da
// colecao) e heroBackdropUrl, depois coverImageUrl, depois o backdropImageUrl da
// colecao; a CAPA do cartaz e coverImageUrl, depois o backdropImageUrl da
// colecao. "" quando nenhum campo veio — nada e inventado.
const char *col_banner(const ColFolder *f);
const char *col_capa(const ColFolder *f);
void col_cor(const ColFolder *f, float *r, float *g, float *b);
// NOME LEGIVEL DE UMA FONTE, para a aba da pagina da colecao: o titulo que a
// conta deu a fonte, senao o nome do catalogo no manifesto (`manifesto`, pode
// ser NULL/""). Vazio quando nenhum dos dois serve — igual ao catId, ou com
// cara de id ("streaming_netflix_movies": underscore e nenhum espaco). Quem
// chama poe so o tipo (Filmes/Series) no lugar.
void col_nome_fonte(const ColSource *s, const char *manifesto, char *dst, unsigned n);
#endif
