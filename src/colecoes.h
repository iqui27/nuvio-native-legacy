#ifndef NV_COLECOES_H
#define NV_COLECOES_H
#define COL_MAX 256
#define COL_SOURCE_MAX 32
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
  char filtros[384];   // objeto "filters" cru; o montador da URL traduz
} ColSource;
typedef struct {
  char id[96], title[128], group[64], cover[512], hero[512], logo[512];
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
  ColSource sources[COL_SOURCE_MAX];
} ColFolder;
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
// da RPC ({collections_json}) e collections_json como STRING escapada. Vazio
// nao apaga as locais (mesma regra dos addons). Devolve quantas pastas entraram.
int col_definir_json(const char *json);
// Chave de fileira de um grupo: collection_<id da colecao> quando a colecao
// tem id (web e catordem usam o id), senao collection_<titulo do grupo>.
void col_chave_grupo(const char *group, char *dst, unsigned n);
int col_n(void);
// Muda sempre que o conjunto de pastas muda. Quem decide remontar a tela deve
// olhar ISTO e nao col_n(): trocar N pastas por outras N mantem o numero.
unsigned col_revisao(void);
const ColFolder *col_folder(int i);
int col_grupo(const char *nome, int *indices, int max);
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
void col_cor(const ColFolder *f, float *r, float *g, float *b);
#endif
