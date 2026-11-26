// TIMESHIFT LOCAL DA TV AO VIVO — as REGRAS, sem disco nem rede.
//
// O fluxo ao vivo que o proxy de TS (proxyts.c) ja entrega a TV tambem vai
// para arquivos de segmento num disco (pendrive). Este cabecalho guarda a
// conta: quanto espaco usar, quando abrir segmento novo, qual apagar, e em que
// byte comeca "N segundos atras". Quem grava e le e timeshift_disco.h.
//
// POSICAO LOGICA: bytes desde o comeco da sessao, a mesma contagem do buffer
// em memoria do proxy. Um segmento e [ini, ini + bytes) e comecou em t0Ms.
//
// Funcoes puras e inline, para o teste compilar sozinho (tests/timeshift_regras.sh).
#ifndef NV_TIMESHIFT_REGRAS_H
#define NV_TIMESHIFT_REGRAS_H
#include <stdint.h>
#include <string.h>

#define TSR_SEG_MAX      2048              // segmentos no indice (anel)
#define TSR_SEG_MS       6000              // duracao minima de um segmento
#define TSR_SEG_BYTES    (64ull << 20)     // um segmento nunca passa disto
#define TSR_MINIMO_BYTES (256ull << 20)    // abaixo disto nao vale ligar
#define TSR_RESERVA_BYTES (512ull << 20)   // folga que fica livre no disco
#define TSR_TETO_BYTES   (4ull << 30)      // teto automatico
#define TSR_PASSO_S      30                // um toque em "Voltar 30 s"
#define TSR_JANELA_MIN_S 30                // janela minima para oferecer a volta
#define TSR_PAUSA_MAX_S  1800              // pausa mais longa que isto encerra a gravacao

typedef struct { uint64_t ini, bytes; int64_t t0Ms; unsigned id; } TsrSeg;
typedef struct {
  TsrSeg seg[TSR_SEG_MAX];
  unsigned prim, n, proxId;   // anel: seg[(prim + i) % TSR_SEG_MAX], i < n
  uint64_t teto, fim;         // orcamento em bytes; fim logico (proximo byte)
} TsrIndice;

// ORCAMENTO: o que o disco tem de livre menos a reserva, ate o teto. 0 = nao
// cabe (o timeshift fica desligado nesta sessao, sem gravar nada).
static inline uint64_t tsr_teto_bytes(uint64_t livre) {
  uint64_t t;
  if (livre < TSR_RESERVA_BYTES + TSR_MINIMO_BYTES) return 0;
  t = livre - TSR_RESERVA_BYTES;
  return t > TSR_TETO_BYTES ? TSR_TETO_BYTES : t;
}
// Quantos segundos cabem em `teto` a `kbps`. 0 sem vazao conhecida.
static inline int tsr_janela_estimada_s(uint64_t teto, int kbps) {
  uint64_t s;
  if (kbps <= 0) return 0;
  s = teto / ((uint64_t)kbps * 125u);
  return s > 86400u ? 86400 : (int)s;
}

static inline void tsr_iniciar(TsrIndice *x, uint64_t teto) {
  memset(x, 0, sizeof *x);
  x->teto = teto;
}
static inline TsrSeg *tsr_seg(TsrIndice *x, unsigned i) { return &x->seg[(x->prim + i) % TSR_SEG_MAX]; }
static inline const TsrSeg *tsr_segc(const TsrIndice *x, unsigned i) { return &x->seg[(x->prim + i) % TSR_SEG_MAX]; }
static inline uint64_t tsr_inicio(const TsrIndice *x) { return x->n ? tsr_segc(x, 0)->ini : x->fim; }
static inline uint64_t tsr_usado(const TsrIndice *x) { return x->fim - tsr_inicio(x); }

// `marco` = 1 num ponto bom de corte (comeco de segmento HLS). Abre segmento
// novo no primeiro byte, num marco depois de TSR_SEG_MS, ou a forca quando o
// segmento em curso chega a TSR_SEG_BYTES (fonte sem marcos).
static inline int tsr_precisa_novo(const TsrIndice *x, int marco, int64_t agoraMs) {
  const TsrSeg *u;
  if (!x->n) return 1;
  u = tsr_segc(x, x->n - 1);
  if (u->bytes >= TSR_SEG_BYTES) return 1;
  return marco && u->bytes > 0 && agoraMs - u->t0Ms >= TSR_SEG_MS;
}
// Abre o segmento e devolve o id (nome do arquivo). Com o indice cheio o mais
// antigo sai: *descartado recebe o id dele e a funcao devolve com *temDescarte.
static inline unsigned tsr_abrir(TsrIndice *x, int64_t agoraMs, unsigned *descartado, int *temDescarte) {
  TsrSeg *s;
  if (temDescarte) *temDescarte = 0;
  if (x->n == TSR_SEG_MAX) {
    if (descartado) *descartado = tsr_seg(x, 0)->id;
    if (temDescarte) *temDescarte = 1;
    x->prim = (x->prim + 1) % TSR_SEG_MAX; x->n--;
  }
  s = tsr_seg(x, x->n++);
  s->ini = x->fim; s->bytes = 0; s->t0Ms = agoraMs; s->id = x->proxId++;
  return s->id;
}
static inline void tsr_somar(TsrIndice *x, uint64_t n) {
  if (!x->n) return;
  tsr_seg(x, x->n - 1)->bytes += n;
  x->fim += n;
}
// Acima do orcamento: tira o mais antigo (nunca o que esta sendo escrito) e
// devolve 1 com o id para apagar. Chamar em laco ate 0.
static inline int tsr_expurgar(TsrIndice *x, unsigned *id) {
  if (x->n < 2 || tsr_usado(x) <= x->teto) return 0;
  *id = tsr_seg(x, 0)->id;
  x->prim = (x->prim + 1) % TSR_SEG_MAX; x->n--;
  return 1;
}

// Quanto da para voltar agora, em ms.
static inline int64_t tsr_janela_ms(const TsrIndice *x, int64_t agoraMs) {
  int64_t j;
  if (!x->n) return 0;
  j = agoraMs - tsr_segc(x, 0)->t0Ms;
  return j > 0 ? j : 0;
}
// Onde comecar para estar `atrasMs` atras: o inicio do ultimo segmento que
// comecou ate la (um comeco de segmento e onde o decodificador engata). Pedido
// alem da janela cai no segmento mais antigo; indice vazio devolve o fim.
static inline uint64_t tsr_pos_atras(const TsrIndice *x, int64_t atrasMs, int64_t agoraMs) {
  int64_t alvo = agoraMs - (atrasMs > 0 ? atrasMs : 0);
  unsigned i;
  uint64_t pos;
  if (!x->n) return x->fim;
  pos = tsr_segc(x, 0)->ini;
  for (i = 1; i < x->n && tsr_segc(x, i)->t0Ms <= alvo; i++) pos = tsr_segc(x, i)->ini;
  return pos;
}
// Em que arquivo esta `*pos`. 1 = achou (*id, *off e *resto = bytes ate o fim
// do segmento); 0 = `*pos` ja e a ponta (esperar mais dado). Posicao que ja
// foi apagada pula para o mais antigo que existe (*pulou = 1).
static inline int tsr_localizar(const TsrIndice *x, uint64_t *pos, unsigned *id, uint64_t *off,
                                uint64_t *resto, int *pulou) {
  unsigned a = 0, b = x->n;
  if (pulou) *pulou = 0;
  if (!x->n) return 0;
  if (*pos < tsr_segc(x, 0)->ini) { *pos = tsr_segc(x, 0)->ini; if (pulou) *pulou = 1; }
  if (*pos >= x->fim) return 0;
  while (b - a > 1) {
    unsigned m = (a + b) / 2;
    if (tsr_segc(x, m)->ini <= *pos) a = m; else b = m;
  }
  // Segmento vazio (acabou de abrir) nunca contem *pos < fim.
  *id = tsr_segc(x, a)->id;
  *off = *pos - tsr_segc(x, a)->ini;
  *resto = tsr_segc(x, a)->bytes - *off;
  return 1;
}

// Passo do botao "Voltar 30 s": soma, sem passar da janela (arredondada para
// baixo ao passo, para o ultimo toque nao pedir o que ja foi apagado).
static inline int tsr_voltar(int atrasS, int janelaS) {
  int max = janelaS - janelaS % TSR_PASSO_S, novo = atrasS + TSR_PASSO_S;
  if (max < TSR_PASSO_S) return atrasS > 0 ? atrasS : 0;
  return novo > max ? max : novo;
}
#endif
