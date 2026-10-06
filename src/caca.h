// CACA A FILMOGRAFIA: as cacadas ativas do perfil, os filmes de cada uma e o
// progresso (plano social-pessoal, secao 8; Onda 2 "Caca").
//
// O QUE E UMA CACADA. Um ALVO que a pessoa escolhe ("Cacar Greta Gerwig") e a
// lista de filmes que o fecha:
//   DIRECAO / ATUACAO  filmografia do TMDB (longas lancados; "+ Documentarios"
//                      e "+ Curtas" entram por escolha; filme de TV nunca);
//   SAGA               uma colecao do TMDB (/collection/<id>);
//   TOP_TMDB           os 250 mais bem avaliados do TMDB (/movie/top_rated);
//   LISTA_TRAKT        uma lista publica do Trakt (o dono escolhe qual em
//                      /v1/trilhas; NAO e o IMDb Top 250, por licenca);
//   TRILHA             uma trilha de curadoria do servidor (/v1/trilhas):
//                      Oscar de Melhor Filme, "Noir essencial"...
//
// DE ONDE VEM A LISTA. Primeiro o Worker (nuvio-recomendacoes:
// /v1/filmografia, que guarda o resultado classificado e com o imdb de cada
// filme por 14 dias, compartilhado entre todo mundo). Sem ele (rota ainda nao
// publicada, sem rede, sem conta), a TV pergunta ao TMDB direto com a chave que
// ja usa na pagina da pessoa, e completa o imdb e a duracao de cada filme numa
// fila lenta, guardada em disco para sempre (caca-meta.txt).
//
// O QUE FOI VISTO vem de cacavisto.h (o adaptador que vira o Diario).
//
// FIOS. Um fio so faz a rede, numa fila; o resultado e ADOTADO pelo fio
// principal em caca_passo. Todas as leituras abaixo sao do fio principal.
#ifndef NV_CACA_H
#define NV_CACA_H
#include <stddef.h>

#define CACA_MAX         12     // cacadas ativas por perfil
#define CACA_FILMES_MAX  260    // o top 250 cabe com folga
#define CACA_PREVIA      CACA_MAX   // indice da "previa" (pagina da pessoa)

typedef enum {
  CACA_NADA = 0, CACA_DIRECAO, CACA_ATUACAO, CACA_SAGA, CACA_TOP_TMDB,
  CACA_LISTA_TRAKT, CACA_TRILHA, CACA_TIPO_N
} CacaTipo;

// Classe do filme dentro de uma filmografia.
enum { CACA_LONGA = 0, CACA_DOC = 1, CACA_CURTA = 2, CACA_TV = 3 };
// Escopo opcional (bits).
enum { CACA_ESC_DOC = 1, CACA_ESC_CURTA = 2 };
// Marcas da cacada (bits).
enum {
  CACA_F_FILEIRA = 1,    // "Fazer fileira": fileira na Home so com o que falta
  CACA_F_COLECAO = 2,    // "Fazer colecao": colecao local com todos os filmes
  CACA_F_PRIVADA = 4,    // "Mostrar aos amigos" DESmarcado (padrao: mostrar)
  CACA_F_SEMEADA = 8     // os degraus que ja existiam ao criar foram guardados
};
// Estado da lista de filmes.
enum { CACA_VAZIA = 0, CACA_CARREGANDO, CACA_PRONTA, CACA_ERRO };

typedef struct {
  int  tipo;            // CacaTipo
  long alvo;            // id do TMDB (pessoa ou colecao); 0 nos outros
  char lista[64];       // id da trilha / lista do Trakt; "" nos outros
  char nome[96];
  char foto[160];       // URL (rosto da pessoa ou poster da saga)
  int  escopo, flags;
  int  estado;          // CACA_VAZIA..CACA_ERRO
  int  vistos, total;   // no escopo
  int  degraus;         // o que o progresso de AGORA da (CONQ_*)
  int  ganhos;          // o que ja foi ganho (nunca diminui)
  int  resolvendo;      // filmes ainda sem imdb (fila lenta), 0 = tudo pronto
  long anoIni, anoFim;
} CacaInfo;

typedef struct {
  long tmdb;
  char imdb[16];
  char titulo[96];
  char poster[160];     // URL w185, ou ""
  long data;            // AAAAMMDD
  int  classe;
  int  visto;
  long long quando;
} CacaFilme;

// --- ciclo de vida ------------------------------------------------------------
void caca_iniciar(void);
// Por quadro (fio principal). `momentoBom` = sem player aberto: e quando o
// aviso de conquista pode sair (depois dos creditos ou na volta a Home).
void caca_passo(unsigned agora, int momentoBom);
// Troca de perfil: as cacadas do anterior saem, as do novo entram do disco.
void caca_perfil_trocado(void);
// Sobe quando qualquer coisa que a tela mostra mudou.
unsigned caca_revisao(void);

// --- as cacadas ---------------------------------------------------------------
int  caca_n(void);
int  caca_info(int i, CacaInfo *saida);          // i = CACA_PREVIA vale
int  caca_n_filmes(int i);                       // so os do escopo, em ordem
int  caca_filme(int i, int j, CacaFilme *saida);
// Comeca uma cacada (ou devolve a que ja existe para o mesmo alvo). -1 cheio.
int  caca_criar(int tipo, long alvo, const char *lista, const char *nome,
                const char *foto);
void caca_parar(int i);
void caca_alternar_escopo(int i, int bit);
// Fileira/colecao/privacidade: alterna a marca e devolve o estado novo.
int  caca_alternar(int i, int marca);
// A cacada para este alvo, ou -1.
int  caca_achar(int tipo, long alvo, const char *lista);
// Chave estavel do alvo ("dir:525", "saga:119", "trilha:oscar"...).
void caca_chave(int tipo, long alvo, const char *lista, char *dst, size_t tam);

// --- previa (pagina da pessoa: "Voce viu 7 de 12 . Cacar") ---------------------
void caca_previa(int tipo, long alvo, const char *nome, const char *foto);
// Vira cacada de verdade. Devolve o indice ou -1.
int  caca_previa_cacar(void);

// --- sugestoes ----------------------------------------------------------------
// Ate `max` cacadas quase fechadas (>= 50% e faltando <= 3), as mais perto
// primeiro. Devolve quantas e os indices.
int  caca_sugestoes(int *idx, int max);

// --- trilhas (catalogo para "+ Nova trilha") ----------------------------------
typedef struct {
  int  tipo;            // CACA_SAGA / CACA_TOP_TMDB / CACA_LISTA_TRAKT / CACA_TRILHA
  long alvo;
  char lista[64];
  char nome[96];        // ja no idioma (o JSON traz um nome por idioma)
  char foto[160];
} CacaTrilha;
int  caca_n_trilhas(void);
int  caca_trilha(int i, CacaTrilha *saida);

// --- fileira da Home ("Fazer fileira") ----------------------------------------
// O que FALTA ver da cacada `i`, no formato do catalogo, para a fonte "caca"
// de descoberta.c. Devolve quantos. `dst` e um CatItem* (catalogo.h).
int  caca_itens(int i, int soFaltam, void *dst, int max);
// Indice da cacada pela chave guardada na fonte (ColSource.catId).
int  caca_por_chave(const char *chave);

// --- puras, para teste --------------------------------------------------------
// Classe de um credito do TMDB: genre_ids e runtime (0 = nao se sabe).
int  caca_classe(const int *generos, int nGeneros, int duracao);
// O filme entra no escopo? (filme de TV nunca; doc/curta por escolha)
int  caca_no_escopo(int classe, int escopo);
// "AAAA-MM-DD" -> AAAAMMDD (0 vazio/invalido).
long caca_data(const char *iso);
// Le o corpo de /person/<id>/movie_credits. `papel` CACA_DIRECAO/CACA_ATUACAO.
// `hoje` AAAAMMDD: o que nao saiu ainda fica fora. Devolve quantos.
int  caca_ler_creditos(const char *json, int papel, long hoje,
                       CacaFilme *dst, int max);
// Le o corpo de /v1/filmografia (Worker) ou de uma lista do formato
// {"filmes":[{tmdb,imdb,titulo,poster,data,classe}]}.
int  caca_ler_filmes(const char *json, CacaFilme *dst, int max);

#endif
