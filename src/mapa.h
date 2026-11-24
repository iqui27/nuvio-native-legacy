#include "arteurl.h"
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
#include <stddef.h>
#include <stdint.h>
#include "farolgosto.h"   // mapa_clima_gosto le o modelo (issue 43)

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
  char poster[NV_ARTE_URL_MAX];     // URL pronta
  char fundo[200];      // backdrop, URL pronta
  char sinopse[300];
  int  ano;             // 0 = desconhecido
  int  nota;            // 0..100 (vote_average * 10)
  int  votos;
  int  catIndice;       // indice em cat_item no momento da montagem; -1 = fora
  char catId[64];       // id cru do item no catalogo ("kitsu:7442"): identidade
                        // de quem nao tem IMDb nem TMDB, para re-resolver o
                        // catIndice depois de uma troca (CATREVB)
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
// VIZINHANCA DE UM TITULO (Explorar 2.0, a "toca do coelho").
//
// O mapa acima e um retrato de 8 sementes. A vizinhanca e a mesma ideia para
// QUALQUER titulo, sob demanda: quatro grupos de elo, cada um com ate
// MAPA_VIZ_ITENS titulos, e nenhum titulo repetido entre grupos.
//
//   posicao           com TMDB                         sem chave / sem rede
//   MAPA_VIZ_PESSOA   /person/<id>/combined_credits    direcao/elenco do catalogo;
//                                                      sem pessoa em comum, a
//                                                      mesma decada (MAPA_GR_EPOCA)
//   MAPA_VIZ_TEMA     /discover com a palavra-chave    genero em comum do catalogo
//                                                      (MAPA_GR_GENERO)
//   MAPA_VIZ_REC      recommendations do titulo        generos + ano do catalogo
//   MAPA_VIZ_AMIGOS   o que os amigos gostaram (amigostitulo.h), nos dois casos
//
// CUSTO. mapa_vizinhos_pedir roda no fio principal e publica NA HORA o retrato
// local (catalogo, sem arquivo, sem rede); com chave do TMDB um fio proprio
// troca os grupos pelos de verdade e publica de novo. Cada titulo fica em cache
// (dados_dir()/explorar-viz.txt, 7 dias, e os ultimos em memoria: subir um
// degrau da trilha nao faz rede nem disco). Quem desenha so copia o retrato
// quando a revisao muda.
#define MAPA_VIZ_GRUPOS 4
#define MAPA_VIZ_ITENS  5
enum { MAPA_VIZ_PESSOA = 0, MAPA_VIZ_TEMA, MAPA_VIZ_REC, MAPA_VIZ_AMIGOS };
enum { MAPA_GR_PESSOA = 0, MAPA_GR_EPOCA, MAPA_GR_TEMA, MAPA_GR_GENERO,
       MAPA_GR_REC, MAPA_GR_AMIGOS };

typedef struct {
  MapaObra obra;
  char motivo[64];      // o "porque" deste titulo: pessoa, tema, genero, decada, amigo
  int  visto;           // a pessoa ja comecou este titulo
} MapaVizItem;

typedef struct {
  int  tipo;            // MAPA_GR_*
  char sub[64];         // rotulo do grupo: nome da pessoa, tema (chave pt), "2000", amigos
  char kw[48];          // MAPA_GR_TEMA: a palavra-chave do TMDB (ingles), para manter o fio
  long ref;             // id da pessoa / da palavra-chave no TMDB; amigos: quantos
  int  n;               // 0 = o grupo nao existe para este titulo
  MapaVizItem itens[MAPA_VIZ_ITENS];
} MapaVizGrupo;

typedef struct {
  unsigned revisao;
  int  carregando;      // 1 enquanto o fio do TMDB trabalha
  int  remoto;          // 1 = os grupos vieram do TMDB (0 = so o catalogo)
  MapaObra foco;        // o titulo pedido, com o que o TMDB completou
  char generos[120];    // "Drama · Mistério", para a linha de meta
  int  focoVisto;
  MapaVizGrupo g[MAPA_VIZ_GRUPOS];
} MapaVizinhos;

// Pede a vizinhanca de `foco`. `pessoaPref` (id do TMDB, 0 = tanto faz) e
// `temaPref` (NULL = tanto faz) mantem o MESMO fio quando a pessoa desce a
// toca por ele. `temaEhGenero` diz o que o fio carrega: 0 = palavra-chave do
// TMDB em ingles (selo de tema ou entrada generica), 1 = nome de genero
// local (selo ou grupo de genero) — nunca vira palavra-chave no retrato
// remoto, so fixa a preferencia de genero (FAROL46C). Repetir o pedido do
// titulo que ja esta publicado nao faz nada.
void mapa_vizinhos_pedir(const MapaObra *foco, long pessoaPref, const char *temaPref,
                         int temaEhGenero);
// Variante com id canonico do genero (> 0 quando veio de um selo ou grupo do
// Mapa): propaga o id TMDB diretamente sem inferir do texto localizado.
void mapa_vizinhos_pedir_genero(const MapaObra *foco, long pessoaPref,
                                const char *temaPref, int temaEhGenero,
                                long generoId);
int  mapa_vizinhos_copiar(MapaVizinhos *dst, unsigned *revisao);
// O titulo `indice` do catalogo como MapaObra (entrada pelo Detalhe). 1 = ok.
int  mapa_obra_do_catalogo(int indice, MapaObra *o);

// ---------------------------------------------------------------------------
// CLIMAS (Explorar 2.0, o portal de entrada). Uma tabela EDITORIAL: cada clima
// tem nome e descricao (chaves de i18n) e uma regra sobre generos e
// palavras-chave do TMDB. O catalogo local alimenta todos na hora; o
// /discover/movie|tv de um clima so e pedido quando ele e ABERTO
// (mapa_clima_abrir), e fica em cache por 7 dias (explorar-climas.txt).
#define MAPA_CLIMA_N      11
#define MAPA_CLIMA_ITENS  24

typedef struct {
  int id;               // posicao na tabela editorial (mapa_clima_nome)
  int total, vistos;    // titulos no clima / quantos a pessoa ja viu
  int afinidade;        // 0..100: parte do que ela viu que cai neste clima
  int gosto;            // -50..+50: o que o modelo do Farol acha dos generos
                        // do clima (soma dos pesos, saturada). So ordena —
                        // a tela continua mostrando `afinidade` (issue 43).
  int remoto;           // 1 = o /discover ja entrou
  int carregando;
  int n;
  MapaObra itens[MAPA_CLIMA_ITENS];
  unsigned char visto[MAPA_CLIMA_ITENS];
} MapaClima;

typedef struct {
  unsigned revisao;
  int n;                        // MAPA_CLIMA_N, ja na ordem de afinidade
  MapaClima c[MAPA_CLIMA_N];
} MapaClimas;

const char *mapa_clima_nome(int id);        // chave pt de i18n()
const char *mapa_clima_descricao(int id);
// Monta os climas do catalogo (fio principal) e ordena por afinidade. A ordem
// e fixada aqui e nao muda quando o /discover chega: azulejo nao pula.
void mapa_climas_pedir(void);
void mapa_clima_abrir(int id);
int  mapa_climas_copiar(MapaClimas *dst, unsigned *revisao);
// Quanto o modelo `g` gosta dos generos da regra do clima `id`, em Q8.8 —
// soma dos pesos de genero no dia `dia` (dias desde 1/1/2026, a epoca do
// farolsinal). Filme e serie somam nas duas colunas do GEN_TMDB; em clima
// soSerie so a coluna de serie conta. Palavras-chave nao entram: o tema do
// modelo e o id do TMDB e a regra editorial so tem o nome. Puro (issue 43).
int  mapa_clima_gosto(const FarolGosto *g, int id, uint16_t dia);

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

// O detalhe do TMDB no recorte que o modelo de gosto usa (farolficha):
// id, numeros e as mesmas listas de mapa_ler_detalhe — duracao e idioma
// original alem dos campos que a tela mostra. `gente` mantem a ordem de la:
// direcao/criacao primeiro.
typedef struct MapaDetalhe {
  long tmdb;                       // id da raiz; 0 = resposta sem id
  int  ano;
  int  minutos;                    // runtime do filme; episode_run_time[0] da serie
  int  nota;                       // vote_average * 10
  int  votos;
  char idioma[8];                  // original_language ("pt", "en", "ja")
  char imdb[24];
  MapaEtiqueta gen[MAPA_GEN_MAX];    int nGen;
  MapaEtiqueta kw[MAPA_KW_MAX];      int nKw;
  MapaPessoa   gente[MAPA_GENTE_MAX]; int nGente;
} MapaDetalhe;

// Le a resposta de /movie|tv/<id>?append_to_response=keywords,credits,
// recommendations. `serie` escolhe os nomes de campo de TV. Preserva o que ja
// havia em `s->obra` quando a resposta nao traz o campo. 1 quando leu.
int  mapa_ler_detalhe(const char *json, int serie, MapaSemente *s);
// Transporte da fila lenta do farolsinal (issue 38): baixa o detalhe TMDB
// do titulo (/movie|tv/<id> com keywords,credits,external_ids), resolvendo
// imdb->tmdb quando so o imdb chega. Copia em `dst` (sem fechar a string),
// devolve os bytes ou 0. Chamavel de um fio de trabalho — faz rede.
int  mapa_detalhe_json(const char *imdb, long tmdb, int serie, char *dst,
                       size_t cap);
// O MESMO JSON de mapa_ler_detalhe, sem obra/poster/sinopse/recomendacoes:
// o que o farolficha precisa para montar a ficha do titulo. Resposta
// truncada devolve o que deu para ler. 1 = leu algo util (id do TMDB, imdb
// ou alguma das listas).
int  mapa_ler_ids(const char *json, int serie, MapaDetalhe *d);
// Id estavel e negativo de um nome sem id do TMDB — a convencao de
// MapaFio.id na reserva local (catalogo). Sempre < 0.
long mapa_hash_nome(const char *nome);
// "Filme · Acao · Drama" do CatItem: pula o primeiro pedaco (o tipo) e vira
// etiquetas de id mapa_hash_nome, como a reserva do mapa sempre fez.
void mapa_generos_locais(const char *genero, MapaEtiqueta *gen, int *nGen, int max);
// Primeiro ano de 4 digitos de `meta` ("2022 · 3 temporadas" -> 2022).
// 0 = nao tem.
int  mapa_ano_meta(const char *meta);
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

// --- vizinhanca e climas: as partes puras --------------------------------------

// Um titulo do catalogo local visto de longe: so o que o cruzamento compara.
// Generos e pessoas vao por hash do NOME (mapa_hash_titulo): o catalogo nao
// tem os ids do TMDB.
typedef struct {
  MapaObra obra;
  unsigned gen[MAPA_GEN_MAX];     int nGen;
  unsigned genMasc;             // os mesmos generos por identidade (mascara MAPA_G_*)
  unsigned gente[MAPA_GENTE_MAX]; int nGente;
} MapaVizCand;
typedef struct { MapaObra obra; char quem[32]; } MapaVizAmigo;

typedef struct {
  const MapaSemente *foco;
  const MapaCreditos *cred;       // creditos da pessoa escolhida, ou NULL
  const char *credNome;
  const MapaObra *tema; int nTema; // /discover da palavra-chave, ou 0
  const char *temaNome;           // em portugues (mapa_tema_nome)
  const char *temaKw;             // a mesma palavra em ingles
  long temaId;
  const MapaVizCand *pool; int nPool;
  const MapaVizAmigo *amigos; int nAmigos;
  const unsigned *vistos; int nVistos;   // hashes de titulo ja comecados
  unsigned pessoaPref;            // hash do nome preferido na reserva local (0 = nenhum)
  unsigned generoPref;            // hash do genero preferido na reserva local (0 = nenhum)
  unsigned generoPrefMasc;        // o mesmo genero por identidade (mascara MAPA_G_*)
  long generoPrefId;              // id TMDB do genero preferido (> 0 quando veio do selo)
  const char *generoPrefNome;     // o rotulo do selo, no idioma do catalogo
  unsigned focoGenMasc[MAPA_GEN_MAX]; // mascaras canonicas dos generos do foco (fio principal)
} MapaVizEntrada;
// Monta os quatro grupos. Preserva v->revisao.
void mapa_vizinhos_montar(const MapaVizEntrada *e, MapaVizinhos *v);

// {"results":[...],"total_results":N} de /discover e afins. `serie` fixa o
// tipo (o /discover nao manda media_type). Devolve quantas obras leu.
int  mapa_ler_lista(const char *json, int serie, MapaObra *o, int max, int *total);
typedef struct {
  MapaObra obra;
  unsigned tituloHash, originalHash;
} MapaRecomendacao;
typedef struct {
  MapaRecomendacao obras[MAPA_REC_MAX];
  MapaEtiqueta generos[MAPA_GEN_MAX];
  int n, nGen;
} MapaRecomendacoes;
int  mapa_ler_recomendacoes(const char *json, int serie, MapaRecomendacoes *saida);
int  mapa_recomendacoes_tmdb(const char *imdb, long tmdb, int serie,
                             const char *chave, const char *idioma, MapaRecomendacoes *saida);
// /search/keyword: o id da palavra de nome EXATO `nome` (sem caixa). 0 = nao ha.
long mapa_ler_keyword_id(const char *json, const char *nome);

// Generos canonicos da regra dos climas e selos (bit = 1 << MAPA_G_*).
enum { MAPA_G_ACAO = 1, MAPA_G_AVENTURA, MAPA_G_COMEDIA, MAPA_G_CRIME, MAPA_G_DRAMA,
       MAPA_G_FAMILIA, MAPA_G_FANTASIA, MAPA_G_FICCAO, MAPA_G_MISTERIO,
       MAPA_G_TERROR, MAPA_G_SUSPENSE,
       MAPA_G_ANIMACAO, MAPA_G_DOCUMENTARIO, MAPA_G_HISTORIA, MAPA_G_MUSICA,
       MAPA_G_ROMANCE, MAPA_G_GUERRA, MAPA_G_FAROESTE,
       MAPA_G_INFANTIL, MAPA_G_NOTICIAS, MAPA_G_REALITY, MAPA_G_NOVELA,
       MAPA_G_TALK, MAPA_G_N };
// Mascara de um nome de genero ("Sci-Fi & Fantasy" liga dois bits). Aceita os
// nomes em ingles, portugues e espanhol que o catalogo e o TMDB mandam.
unsigned mapa_genero_mascara(const char *nome);
// 1 = o titulo cai no clima `id`. `kws` sao palavras-chave do TMDB em ingles;
// com nKw == 0 (titulo do catalogo, sem palavras) vale a regra de generos.
int  mapa_clima_casa(int id, unsigned generos, const char *const *kws, int nKw, int serie);
// Quantas palavras-chave a regra do clima usa, e a i-esima (ingles).
int  mapa_clima_n_kw(int id);
const char *mapa_clima_kw(int id, int i);
// A parte "with_genres=..&with_keywords=a|b" do /discover. `ids` sao os ids
// das palavras da regra ja resolvidos (0 = nao resolvida, fica de fora).
// 0 = este clima nao tem consulta para esse tipo (ex.: so serie, pedindo filme).
int  mapa_clima_consulta(int id, int serie, const long *ids, int nIds, char *dst, size_t n);

#endif
