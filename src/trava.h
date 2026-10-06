// TRAVA DE SERIE — perfis da mesma conta (e, numa onda futura, amigos de
// outras TVs) andando juntos numa serie. Plano: docs/plans/social-pessoal/
// social-pessoal-plano.md, secao 5. Estado: docs/plans/social-pessoal/
// STATUS-trava.md.
//
// DOIS MODOS:
//   JUNTOS     um episodio que conta como visto e marcado para TODOS os
//              membros, nao importa qual perfil estava na TV. Ninguem espera.
//   SEPARADOS  cada um assiste no proprio perfil; quem esta a frente espera.
//              O LIMITE de cada membro e calculado pelos OUTROS: da para ver o
//              proximo episodio depois do ponto de cada um deles, nao o
//              seguinte. Passar a frente ("Assistir mesmo assim") nao quebra
//              nada, porque o limite dos outros nao depende de quem passou.
//
// ESTE ARQUIVO E SO O MODELO: a regra pura (sem SDL, sem rede, testada em
// tests/trava.c), o estado em memoria e o arquivo travas-<conta>.txt. Quem fala
// com o servidor e com a conta e travarede.c; quem desenha e travaui.c.
//
// O MESMO MODELO SERVE AS TRES SITUACOES do plano (mesma TV, mesma conta em
// varias TVs, amigos): um membro e uma `pessoa` do servico social
// ("nuvio:<sub>" para o perfil principal, "nuvio:<sub>:<i>" para os outros,
// a regra de comPerfil em servidor/recomendacoes/src/index.js) e, quando e da
// mesma conta, tambem o `perfil` (profile_index). Amigo = perfil 0. A onda de
// amigos so precisa preencher `estado` = convidado e o aceite; a regra do
// limite ja ignora quem nao esta "dentro".
//
// FIO PRINCIPAL em tudo que muda estado; as leituras para a rede copiam sob o
// mutex interno (trava_copiar).
#ifndef NV_TRAVA_H
#define NV_TRAVA_H

#define TRAVA_MAX       30     // travas ativas por conta (o teto do servidor)
#define TRAVA_MEMBROS    8     // membros por trava (o teto do servidor)
#define TRAVA_ID_MAX    40
#define TRAVA_PESSOA_MAX 96

enum { TRAVA_JUNTOS = 0, TRAVA_SEPARADOS = 1 };
enum { TRAVA_CONVIDADO = 0, TRAVA_DENTRO = 1, TRAVA_SAIU = 2 };

typedef struct {
  char pessoa[TRAVA_PESSOA_MAX];  // id do servico social; "" = ainda sem conta
  int  perfil;                    // profile_index na conta; 0 = amigo (onda 4)
  int  estado;                    // TRAVA_CONVIDADO | DENTRO | SAIU
  int  t, e;                      // o episodio MAIS ADIANTADO visto (0,0 = nao comecou)
  long long em;                   // quando chegou nele (s)
} TravaMembro;

typedef struct {
  char id[TRAVA_ID_MAX];          // do servidor; "l-<hex>" enquanto so local
  char serie[24];                 // "tt..." (id do titulo, sem :T:E)
  char titulo[160];
  char poster[400];
  int  modo;                      // TRAVA_JUNTOS | TRAVA_SEPARADOS
  int  pin;                       // 1 = "Assistir mesmo assim" pede PIN
  int  pausada;                   // ferias: ninguem espera, nada se desfaz
  char criador[TRAVA_PESSOA_MAX];
  long long criado, atualizado;
  int  local;                     // 1 = ainda nao confirmada pelo servidor
  int  n;
  TravaMembro m[TRAVA_MEMBROS];
} Trava;

// O que a trava precisa saber de cada perfil da conta (copiado de perfis.c
// pelo travarede.c; o teste preenche a mao).
typedef struct {
  int  indice;
  int  primario;
  int  temPin;
  int  infantil;
  char nome[64];
  char avatar[300];
  char cor[10];
} TravaPerfil;
#define TRAVA_PERFIS_MAX 8

// ---------------------------------------------------------------- regra pura
// Ordem por (temporada, episodio). -1, 0, 1.
int  trava_ep_cmp(int t1, int e1, int t2, int e2);
// Quantos episodios uma temporada tem; 0 = nao se sabe. Usado para saber se o
// "proximo" depois do ultimo de uma temporada e o E1 da seguinte.
typedef int (*TravaNEps)(void *ctx, int temporada);
// O membro `m` (a posicao dele) DEIXA ver (t, e)? Sim quando (t, e) e o mesmo
// ponto ou anterior, ou o proximo episodio depois dele. Quem nao comecou deixa
// so o T1E1. Especial (temporada 0) sempre deixa.
int  trava_membro_deixa(const TravaMembro *m, int t, int e, TravaNEps nEps, void *ctx);
// (t, e) esta liberado para o membro de indice `eu`? Juntos, pausada, especial
// ou ja visto pelo proprio (`jaVisto`, a regra do rever) = sempre. Separados =
// todo outro membro DENTRO deixa. `quem` (opcional) recebe os indices de quem
// segura, `nQuem` quantos. Devolve 1 liberado, 0 esperando.
int  trava_liberado(const Trava *tv, int eu, int t, int e, int jaVisto,
                    TravaNEps nEps, void *ctx, int *quem, int *nQuem);
// O LIMITE de `eu`: o ultimo episodio que ele pode ver (o proximo depois do
// membro mais atrasado). Juntos/pausada/sozinho = 0 (sem limite). 1 com limite.
int  trava_limite(const Trava *tv, int eu, TravaNEps nEps, void *ctx, int *t, int *e);
// Indice do membro deste perfil (mesma conta), ou -1.
int  trava_membro_do_perfil(const Trava *tv, int perfil);
// Membros dentro (estado DENTRO).
int  trava_n_dentro(const Trava *tv);

// LINHA DE ESTADO (T1 e T6). Monta em `dst` a frase JA TRADUZIDA, vista por
// `eu`. `nomeDe(ctx, i)` da o nome curto do membro i. Formas:
//   ninguem comecou            "Vocês ainda não começaram."
//   todos no mesmo ponto       "Vocês estão no mesmo episódio, T1E1."
//   um atras de voce           "Sandra está 2 episódios atrás: você espera por ela no T1E5."
//   varios atras               "Você espera por Jerry e Sandra no T1E5."
//   voce atras                 "Você está atrás: Jerry está no T1E5."
//   juntos                     "Juntos: o próximo é o T1E5 para todos."
// Devolve o tipo da frase (TRAVA_FRASE_*), para o teste e para a cor.
typedef const char *(*TravaNome)(void *ctx, int i);
enum { TRAVA_FRASE_NADA = 0, TRAVA_FRASE_NAO_COMECARAM, TRAVA_FRASE_MESMO,
       TRAVA_FRASE_ESPERA_UM, TRAVA_FRASE_ESPERA_VARIOS, TRAVA_FRASE_ATRAS,
       TRAVA_FRASE_JUNTOS, TRAVA_FRASE_PAUSADA };
int  trava_frase(const Trava *tv, int eu, TravaNome nomeDe, void *ctx,
                 char *dst, unsigned tam);
// "Esperando Jerry", "Esperando Jerry e Sandra", "Esperando Jerry e mais 2".
void trava_esperando_texto(const int *quem, int n, TravaNome nomeDe, void *ctx,
                           char *dst, unsigned tam);
// "T1E5" no idioma da TV.
void trava_ep_texto(int t, int e, char *dst, unsigned tam);

// ---------------------------------------------------------------- estado
// A conta (sub) e os perfis dela. Trocar de conta recarrega o arquivo da nova.
void trava_definir_conta(const char *sub);
const char *trava_conta(void);
void trava_definir_perfis(const TravaPerfil *v, int n, int ativo);
int  trava_n_perfis(void);
const TravaPerfil *trava_perfil(int i);           // i em [0, n)
const TravaPerfil *trava_perfil_indice(int indice);
int  trava_perfil_ativo(void);
// "nuvio:<sub>" ou "nuvio:<sub>:<i>" (a regra de comPerfil do servidor).
void trava_pessoa_do_perfil(int indice, char *dst, unsigned tam);

int  trava_n(void);
const Trava *trava_i(int i);
const Trava *trava_por_id(const char *id);
// Travas desta serie em que o perfil esta DENTRO. Devolve quantas; `idx`
// recebe os indices (trava_i).
int  trava_da_serie(const char *serie, int perfil, int *idx, int max);

// Cria (local, pendente do servidor). `perfis` sao os profile_index dos
// membros; o ativo entra sempre. Devolve o indice, ou -1 (teto, serie vazia,
// menos de dois membros).
int  trava_criar(const char *serie, const char *titulo, const char *poster,
                 int modo, int pin, const int *perfis, int n);
// Gestao. Devolve 1 quando mudou.
int  trava_definir_modo(const char *id, int modo);
int  trava_definir_pin(const char *id, int pin);
int  trava_definir_pausada(const char *id, int pausada);
int  trava_definir_membros(const char *id, const int *perfis, int n);
int  trava_apagar(const char *id);
// O perfil chegou a (t, e) nesta serie: anda a posicao dele em TODAS as
// travas da serie (so avanca; `desfazer` = 1 volta para (t, e) exato, que e o
// Desfazer da ilha). Devolve quantas travas mudaram.
int  trava_passo(const char *serie, int perfil, int t, int e, int desfazer);
// Posicao vinda de fora (conta do membro, servidor). So avanca.
int  trava_posicao(const char *id, int perfil, int t, int e, long long em);

// ---------------------------------------------------------------- servidor
// Troca o conteudo pelo que o servidor mandou (GET /v1/travas pedido COMO o
// perfil `perfil`). So as travas desse perfil sao substituidas: as dos outros
// perfis da casa (que esta resposta nao lista) ficam, para a TV funcionar sem
// rede depois de trocar de perfil. As ainda locais (nunca confirmadas) e as
// com operacao na fila ficam; posicoes locais mais adiantadas que as do
// servidor tambem (o passo pode estar na fila). Devolve 1 se leu.
int  trava_aplicar_servidor(const char *json, int perfil);
// A trava local `idLocal` virou `idServidor` (resposta do POST /v1/trava).
void trava_confirmar(const char *idLocal, const char *idServidor);
// ETag da ultima leitura e a revisao (travaRev de /v1/rec).
const char *trava_etag(void);
void trava_definir_etag(const char *etag);
long long trava_rev(void);
void trava_definir_rev(long long rev);

// FILA DE OPERACOES para o servidor (persistida junto). Cada gesto vira uma
// linha; travarede.c envia em ordem e tira com trava_fila_feita.
typedef struct {
  char op[12];                    // "criar" "modo" "pin" "pausar" "membros" "apagar" "passo"
  char id[TRAVA_ID_MAX];
  char pessoa[TRAVA_PESSOA_MAX];  // so "passo": de quem e a posicao
  int  perfil;                    // QUEM FEZ o gesto (o X-Nuvio-Perfil do envio):
                                  // o ativo pode ter mudado ate a fila andar
  int  a, b, c;                   // passo: t, e, desfazer; modo/pin/pausar: valor
} TravaOp;
#define TRAVA_FILA_MAX 64
int  trava_fila_n(void);
int  trava_fila_ver(int i, TravaOp *op);
void trava_fila_feita(int n);     // tira as `n` primeiras
// Tira a PRIMEIRA operacao igual a `op` (op, id, pessoa, a, b, c). A fila pode
// ter mudado enquanto o fio enviava; tirar por igualdade nao tira a errada.
int  trava_fila_tirar(const TravaOp *op);

// "ALCANCOU VOCE": membros que, desde a ultima chamada, alcancaram o perfil
// ativo numa trava em que ele esperava. Devolve 1 e preenche um evento.
typedef struct { char id[TRAVA_ID_MAX]; int membro; int t, e; } TravaAlcance;
int  trava_alcance_proximo(TravaAlcance *ev);

// Mudou algo (para as telas refazerem cache barato).
unsigned trava_geracao(void);

// Disco. trava_carregar e chamado por trava_definir_conta; gravar e automatico
// a cada mudanca. trava_esquecer = logout (o arquivo fica, como o do jornal).
void trava_carregar(void);
void trava_esquecer(void);
// Teste: relogio fixo (s).
void trava_relogio(long long (*f)(void));

#endif
