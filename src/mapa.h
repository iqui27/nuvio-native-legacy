// MAPA DO GOSTO: o que a pessoa viu, cruzado pelo que as historias tem em
// comum. E a camada de DADOS da tela Explorar (explorar.c so desenha).
//
// DE ONDE VEM CADA COISA
//   Sementes  progresso local (prog_ler, mais recente primeiro), itens do
//             catalogo com progresso ou na lista, salvos.txt. Sem nada disso,
//             os titulos mais fortes do catalogo — o mapa nunca abre vazio.
//   Cruzes    TMDB, uma chamada por semente com append_to_response de
//             keywords, credits e recommendations (+ /find para sair do IMDb).
//             A pessoa que atravessa duas sementes ganha um /combined_credits.
//   Reserva   sem chave do TMDB (ou sem rede), o mesmo cruzamento roda so com o
//             que o catalogo ja tem: generos, direcao e elenco dos CatItem, e o
//             proprio catalogo como fonte de candidatos.
//
// CUSTO. A montagem local roda no fio principal e e barata (dezenas de itens,
// sem arquivo, sem rede). Tudo que faz rede ou le/grava o cache em disco roda
// num fio proprio; o desenho so copia o retrato publicado quando a revisao
// muda. O cache (dados_dir()/explorar-mapa.txt) guarda o JA PARSEADO de cada
// semente por 7 dias, entao a segunda abertura nao faz rede nenhuma.
#ifndef NV_MAPA_H
#define NV_MAPA_H

#define MAPA_SEM_MAX    8
#define MAPA_PONTE_MAX  6
#define MAPA_FIO_MAX    2
#define MAPA_TEMA_MAX   5
#define MAPA_SORTE_MAX  16

#define MAPA_KW_MAX     20
#define MAPA_GEN_MAX    6
#define MAPA_GENTE_MAX  10
#define MAPA_REC_MAX    16
#define MAPA_CRED_MAX   6

typedef struct {
  char imdb[24];        // "tt..." quando se sabe
  long tmdb;            // 0 = desconhecido
  char tipo[8];         // "movie" | "series"
  char titulo[120];
  char poster[200];     // URL pronta
  char fundo[200];      // backdrop, URL pronta
  char sinopse[300];
  int  ano;             // 0 = desconhecido
  int  nota;            // 0..100 (vote_average * 10)
  int  votos;
  int  catIndice;       // indice em cat_item no momento da montagem; -1 = fora
} MapaObra;

// O que liga duas sementes, do mais especifico ao mais vago.
enum {
  MAPA_ELO_NADA = 0,
  MAPA_ELO_TEMA,        // palavra-chave do TMDB em comum ("viagem no tempo")
  MAPA_ELO_PESSOA,      // mesma pessoa na direcao ou no elenco
  MAPA_ELO_GENERO,      // mesmo genero
  MAPA_ELO_DUPLA,       // o TMDB recomenda Z a partir das DUAS
  MAPA_ELO_DECADA       // so a mesma decada
};

enum {
  MAPA_ORIGEM_VISTO = 0,  // progresso >= 90%
  MAPA_ORIGEM_ANDAMENTO,
  MAPA_ORIGEM_LISTA,
  MAPA_ORIGEM_ALTA        // sem historico: o melhor do catalogo
};

typedef struct {
  int a, b;             // sementes; a == b quando so ha uma
  MapaObra obra;        // a historia que cruza as duas
  int elo;              // MAPA_ELO_*
  char motivo[64];      // o valor do elo: tema, nome, genero, decada
  int forca;
} MapaPonte;

typedef struct {
  long id;              // TMDB da pessoa; negativo = so nome (reserva local)
  char nome[64];
  char foto[200];
  int  direcao;         // 1 = direcao/criacao, 0 = atuacao
  int  sementes[MAPA_SEM_MAX];
  int  n;
  MapaObra proxima;     // o proximo titulo DELA que a pessoa ainda nao viu
  int  temProxima;
} MapaFio;

typedef struct {
  char nome[48];
  int  n;
  unsigned mascara;     // bit i = semente i
} MapaTema;

enum { MAPA_VAZIO = 0, MAPA_LOCAL, MAPA_CRUZADO };

typedef struct {
  unsigned revisao;
  int estado;           // MAPA_VAZIO | MAPA_LOCAL | MAPA_CRUZADO
  int carregando;       // 1 enquanto o fio do TMDB trabalha
  int nSem;
  MapaObra sem[MAPA_SEM_MAX];
  int semOrigem[MAPA_SEM_MAX];
  int nPontes;          // pontes[0] e a "proxima historia" do centro
  MapaPonte pontes[MAPA_PONTE_MAX];
  int nFios;
  MapaFio fios[MAPA_FIO_MAX];
  int nTemas;
  MapaTema temas[MAPA_TEMA_MAX];
  int nSorte;
  MapaObra sorte[MAPA_SORTE_MAX];
  int anoMin, anoMax;
} Mapa;

// Monta o mapa local na hora (fio principal) e, havendo chave do TMDB, dispara
// o fio que cruza de verdade. Chamar ao abrir a tela. Repetir com as mesmas
// sementes nao refaz rede.
void mapa_pedir(void);

// Copia o retrato publicado quando a revisao e diferente de `*revisao`.
// 1 quando copiou (e atualiza `*revisao`).
int  mapa_copiar(Mapa *dst, unsigned *revisao);

// Logout: apaga o cache em disco e o retrato em memoria.
void mapa_esquecer(void);

// ---------------------------------------------------------------------------
// DAQUI PARA BAIXO: partes puras, expostas para tests/mapa.c.

typedef struct { long id; char nome[48]; } MapaEtiqueta;
typedef struct { long id; char nome[64]; char foto[200]; int direcao; } MapaPessoa;
typedef struct { MapaObra o; long generos[MAPA_GEN_MAX]; int nGen; } MapaRec;

typedef struct {
  MapaObra obra;
  int origem;
  long long quando;                     // time() da leitura no TMDB; 0 = local
  MapaEtiqueta gen[MAPA_GEN_MAX]; int nGen;
  MapaEtiqueta kw[MAPA_KW_MAX];   int nKw;
  MapaPessoa gente[MAPA_GENTE_MAX]; int nGente;
  MapaRec rec[MAPA_REC_MAX];      int nRec;
} MapaSemente;

typedef struct {
  long pessoa;
  MapaObra obras[MAPA_CRED_MAX];
  int n;
} MapaCreditos;

// Le a resposta de /movie|tv/<id>?append_to_response=keywords,credits,
// recommendations. `serie` escolhe os nomes de campo de TV. Preserva o que ja
// havia em `s->obra` quando a resposta nao traz o campo. 1 quando leu.
int  mapa_ler_detalhe(const char *json, int serie, MapaSemente *s);
// /person/<id>/combined_credits: ate MAPA_CRED_MAX obras, as mais votadas,
// do departamento certo (crew/Director quando `direcao`, senao cast).
int  mapa_ler_creditos(const char *json, int direcao, MapaCreditos *c);
// O cruzamento. `vistos` sao hashes de titulo (mapa_hash_titulo) que nunca
// podem virar sugestao — o que a pessoa ja comecou fora das sementes.
void mapa_cruzar(const MapaSemente *s, int n, const MapaCreditos *cred,
                 int nCred, const unsigned *vistos, int nVistos, Mapa *m);
unsigned mapa_hash_titulo(const char *titulo);
// Palavra-chave do TMDB (sempre em ingles) em portugues, que e a chave de
// i18n(). NULL quando a palavra nao esta na tabela: melhor nao mostrar do que
// misturar ingles cru numa frase em portugues.
const char *mapa_tema_nome(const char *kw);

// So para capturas (tests/explorar_shot.c): publica um cruzamento pronto, sem
// rede, como se o fio do TMDB tivesse terminado.
void mapa_publicar_teste(const MapaSemente *s, int n, const MapaCreditos *c, int nc);

#endif
