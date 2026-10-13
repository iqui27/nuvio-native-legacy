// Filmografia 2.1: a REGRA da ficha da pessoa, sem SDL, rede ou desenho.
// Porte de docs/prototypes/filmografia/model.mjs; o teste (tests/filmografia.sh)
// linka so este arquivo e conquistasregra.c.
//
// O que esta aqui: juntar creditos de elenco e equipe, ruido, marcos, capitulos,
// estado de visto (serie so conta com todos os episodios da pessoa), conquistas,
// filtros da grade e o quiz do "Ja vi" com a lista de bloqueio de spoiler. De onde
// vem cada dado fica em filmo.c / pessoa.c. Os textos que saem daqui (rotulos,
// nomes de marco) sao pt-BR; quem desenha passa pelo i18n.
#ifndef NV_FILMOMODELO_H
#define NV_FILMOMODELO_H
#include <stddef.h>

#define FM_MAX       512
#define FM_TIT       120
#define FM_PAP       120
#define FM_COM_MAX     6   // colegas de elenco guardados por titulo
#define FM_KW_MAX      8
#define FM_MARCOS_MAX 10
#define FM_CAP_MAX     6   // teto de capitulos da Historia

enum { FM_ATUA = 1, FM_DIR = 2, FM_ROT = 4 };

// Um credito como o TMDB entrega: elenco e equipe em linhas separadas.
typedef struct {
  long tmdb;
  int  tv;                 // 0 filme, 1 serie
  int  papel;              // FM_ATUA / FM_DIR / FM_ROT
  char titulo[FM_TIT];
  char personagem[FM_PAP];
  int  ano;                // 0 = sem data
  char lancamento[11];     // AAAA-MM-DD ou vazio
  int  ordem;              // posicao no elenco (so atuacao); 99 se nao ha
  int  votos;
  int  eps, epsTotal;      // episodios com a pessoa / da serie
  int  talk;               // genero Talk/News/Reality
  char poster[160];
  char imdb[16];
} FmBruto;

// Um titulo da pessoa, com os papeis dela ja somados.
typedef struct {
  long tmdb;
  int  tv, papeis, ano, ordem, votos, eps, epsTotal;
  int  proximo;            // lancamento futuro
  int  ruido;              // 0 nao; 1 aparicao como si; 2 participacao especial
  int  voz;
  char titulo[FM_TIT], personagem[FM_PAP], lancamento[11], poster[160], imdb[16];
  int  nCom;
  char com[FM_COM_MAX][40];   // colegas de elenco (parcerias e quiz)
  int  nKw;
  char kw[FM_KW_MAX][40];     // temas do TMDB; o quiz filtra spoiler na hora
} FmCredito;

// Junta por (tipo, id do TMDB). `hoje` = "AAAA-MM-DD". Devolve quantos titulos.
int  fm_juntar(const FmBruto *b, int n, const char *hoje, FmCredito *out, int max);
int  fm_ruido(const FmCredito *c);
int  fm_voz(const char *personagem);
// Personagem sem o "(voice)" no fim.
void fm_personagem(const FmCredito *c, char *dst, size_t n);
// "Direcao . Roteiro . Protagonista". `tr` traduz cada pedaco (NULL = pt-BR cru).
void fm_rotulo(const FmCredito *c, char *dst, size_t n, const char *(*tr)(const char *));

// ---- Marcos e capitulos.
enum { FM_M_PRIMEIRA, FM_M_ESTREIA, FM_M_PROTAG, FM_M_DIRECAO, FM_M_ROTEIRO, FM_M_MAIS_VOTADO,
       FM_M_RECENTE, FM_M_AGORA };
#define FM_C_UNICO 99      // capitulo de quem tem um titulo so
typedef struct { int tipo, ano, idx; } FmMarco;
typedef struct { int tipo, de, ate; } FmCap;
// Um marco por titulo, em ordem de ano. Ignora ruido e titulos sem ano; `idx` aponta em `l`.
int  fm_marcos(const FmCredito *l, int n, FmMarco *out, int max);
int  fm_capitulos(const FmCredito *l, int n, const FmMarco *m, int nm, FmCap *out, int max);
const char *fm_marco_nome(int tipo);
const char *fm_capitulo_nome(int tipo);

// ---- Visto.
enum { FM_NAO = 0, FM_ANDA, FM_VISTO, FM_EM_BREVE };
enum { FM_COMO_QUIZ = 1, FM_COMO_DIRETO = 2 };
typedef struct {
  int  estado, pct, epsVistos, epsDe, manual, como;
  char em[11];             // data do visto (historico ou a enviada na marca); vazio se nao ha
} FmEstado;
// `vistoFilme`: 0 nao, 1..99 andamento (%), >=100 visto. `epsVistos` < 0 = filme; >= 0 = episodios
// da pessoa que a conta ja viu. `manualComo` 0 = sem marca manual.
FmEstado fm_estado(const FmCredito *c, int vistoFilme, int epsVistos, const char *emHist,
                   int manualComo, const char *manualEm);
// Conquistas aceitam historico e marca com quiz; marca direta so aparece como visto.
int  fm_conta(const FmEstado *s);

typedef struct { int visto, andamento, total; int papelVisto[3], papelTotal[3]; } FmResumo;
// `st[i]` e o estado de l[i]. Conta so o nucleo (sem ruido e sem futuro).
void fm_resumo(const FmCredito *l, const FmEstado *st, int n, FmResumo *r);

// ---- Conquistas (mesma regra de cqregra_nivel).
#define FM_CQ_MAX 4
typedef struct { char chave[40]; char nome[64]; int trilha, visto, total, nivel; } FmConquista;
int  fm_conquistas(long tmdb, const char *curto, const FmCredito *l, const FmEstado *st, int n,
                   FmConquista *out);
// Quantas conquistas subiram de nivel entre `antes` e `depois`.
int  fm_subiu(const FmConquista *antes, int na, const FmConquista *depois, int nd);
// Quantos vistos pedem o nivel.
int  fm_precisa(int nivel, int total);
const char *fm_nivel_nome(int nivel);

// ---- Grade.
typedef struct { int ordem, tipo, papel, status, ano; } FmFiltro;   // ano 0 = todos
enum { FM_O_NOVOS, FM_O_ANTIGOS, FM_O_VOTOS, FM_O_N };
enum { FM_T_TODOS, FM_T_FILME, FM_T_SERIE, FM_T_N };
enum { FM_P_TODOS, FM_P_ATUA, FM_P_DIR, FM_P_ROT, FM_P_VOZ, FM_P_N };
enum { FM_S_TODOS, FM_S_VISTOS, FM_S_NAO, FM_S_N };
// Gira a opcao de um filtro (0 ordem, 1 tipo, 2 papel, 3 status).
void fm_girar(FmFiltro *f, int qual);
// Indices de `l` que passam no filtro, na ordem pedida. Devolve quantos.
int  fm_filtrar(const FmCredito *l, const FmEstado *st, int n, const FmFiltro *f, int *idx, int max);

// ---- Proximo a ver.
enum { FM_PROX_CONTINUAR = 1, FM_PROX_FECHA, FM_PROX_VOTADO };
// Indice em `l` (ou -1). `prefer` = FM_ATUA / FM_DIR. `cq` = conquistas atuais: se faltar 1 titulo
// para o proximo nivel, ele vence e `*nivel` traz o nivel que fecha.
int  fm_proximo(const FmCredito *l, const FmEstado *st, int n, int prefer,
                const FmConquista *cq, int ncq, int *motivo, int *nivel);

// ---- Quiz do "Ja vi".
#define FM_QOPC 4
#define FM_Q_MAX 8
enum { FM_Q_PERSONAGEM, FM_Q_COLEGA, FM_Q_TEMA, FM_Q_CENA };
// Cena: `cena[i]` 0 = imagem do proprio titulo, 1..3 = imagens de titulos parecidos.
typedef struct { int tipo, certa; char opc[FM_QOPC][48]; int cena[FM_QOPC]; } FmPergunta;
typedef struct {
  FmPergunta q[FM_Q_MAX];
  int nq;                  // perguntas montadas (o quiz so abre com 2 ou mais)
  int nAsk;                // 2, ou 3 depois do desempate
  int ordem[3];            // qual pergunta esta em cada posicao
  int proxReserva;
  int pergunta, acertos, trocou, bonus, resultado;   // resultado 0 aberto, 1 passou, 2 nao
} FmQuiz;
// Termo que nunca pode virar pergunta (final, reviravolta, morte, assassino, traicao...).
int  fm_spoiler(const char *termo);
// `cenaOk` 0 quando o servidor bloqueou a cena do titulo. `decoyP` / `decoyC` / `decoyT` sao
// opcoes erradas (personagens / pessoas / temas) que nao estao nos creditos deste titulo.
void fm_quiz_montar(FmQuiz *z, const FmCredito *c, const char *pessoa, int cenaOk,
                    const char *const *decoyP, int nDP, const char *const *decoyC, int nDC,
                    const char *const *decoyT, int nDT, unsigned semente);
const FmPergunta *fm_quiz_pergunta(const FmQuiz *z);   // NULL quando o quiz terminou
void fm_quiz_responder(FmQuiz *z, int escolha);
// "Nao lembro desta": troca por uma reserva, uma vez por tentativa. 1 se trocou.
int  fm_quiz_trocar(FmQuiz *z);
// Tentativas restantes na janela de 24 h. `quando` = instantes das falhas (segundos).
#define FM_TENTATIVAS 2
#define FM_JANELA_S 86400L
int  fm_tentativas(const long *quando, int n, long agora);
// Data enviada: a de estreia so se a pessoa nao lembra e a estreia ja passou; senao hoje.
void fm_data_vista(const char *lancamento, int naoLembro, const char *hoje, char *dst, size_t n);

unsigned fm_hash(const char *s);

#endif
