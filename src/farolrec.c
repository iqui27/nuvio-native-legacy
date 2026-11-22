// Motor de recomendacao do Farol — ver farolrec.h (lote sense, issue 41;
// secao 3 do PLANO-GOSTO). Modulo puro: sem SDL, sem rede, sem malloc e sem
// relogio — `dia`/`faixaHora` vem no contexto e o sorteio vem de
// `ctx->semente`. Toda a memoria de trabalho fica na pilha da chamada:
// tabelas hash do modelo (~4,3 KB) + vetores por posicao elegivel (~21 KB)
// + tabela de dedup (~10 KB) — 38 KB medidos (gcc -fstack-usage), detalhado
// em farolrec_escolher.

#include "farolrec.h"
#include <string.h>

_Static_assert(sizeof(FarolRecCand) == 48,
               "FarolRecCand fora dos 48 bytes da secao 3.1");
_Static_assert(FAROL_REC_CAND_MAX == 400, "teto de candidatos da secao 3.1");

// Pesos da formula 3.2 em Q15 (32768 = 1,0). Os termos chegam em Q8.8
// [-256, +256]; o produto sai em Q23 e o >> 8 devolve Q15.
#define FR_W_GENERO  9830   // 0,30
#define FR_W_TEMA    8192   // 0,25
#define FR_W_PESSOA  6554   // 0,20
#define FR_W_DECADA  1638   // 0,05
#define FR_W_IDIOMA  1638   // 0,05
#define FR_W_HORA    3277   // 0,10
#define FR_W_QUAL    1638   // 0,05
#define FR_B_SEMENTE 3277   // +0,10 rec do TMDB da semente
#define FR_B_PESSOA  3277   // +0,10 proxima obra de pessoa seguida
#define FR_B_SALVO   4915   // +0,15 salvos / watchlist
#define FR_B_AMIGO   3277   // +0,10 amigo com gosto parecido
#define FR_B_REC     6554   // +0,20 recomendacao recebida
#define FR_P_MOSTR   6554   // -0,20 mostrado em >= 3 dias
#define FR_AMIGO_MIN 70     // afinidade minima do F_AMIGO (tabela 3.1)

// MMR (3.3): nota' = 0,7*pontos - 0,3*simMax. Sim em Q15: 1,0 = mesma
// semente ou franquia; 0,5 = mesmo genero principal ou pessoa em comum.
#define FR_LAMBDA   22938   // 0,7 * 32768
#define FR_UMENOSL   9830   // 0,3 * 32768
#define FR_SIM_MESMO 32768
#define FR_SIM_MEIO  16384

// Media bayesiana da qualidade (3.2): prior 6,8 com 200 votos.
#define FR_QUAL_PRIOR   68   // nota x10
#define FR_QUAL_VOTOS  200

// Regras duras (3.3): 2 por semente, 3 do mesmo genero principal entre os
// 6 primeiros, 1 por franquia; fora da curva nas posicoes 5-8 (0..4..7).
#define FR_MAX_SEMENTE 2
#define FR_MAX_GENERO6 3
#define FR_JANELA_GEN  6
#define FR_FC_INI      4
#define FR_FC_FIM      7

// Tabela de dedup por chave (enderecamento aberto): 4x o teto de candidatos
// mantem a sondagem em ~1 tentativa.
#define FR_SET_N 1024

// Peso projetado de uma afinidade, guardado na tabela hash da dimensao no
// inicio da chamada: depois disso cada consulta e ~1 sondagem, sem varrer o
// vetor esparso do modelo por candidato (meta de tempo da 3.2).
typedef struct { int32_t id; int16_t peso; uint16_t n; } FRW;

// Uma tabela hash por dimensao, abertas num vetor so: cada uma e potencia
// de 2 com ~2x o cap da dimensao, entao a carga fica abaixo de 60 % e a
// sondagem termina em ~1 tentativa. Slot com id 0 = vazio (o modelo nunca
// guarda id 0 — mesma convencao do farolgosto).
#define FR_HSH_N 544
static const struct { uint16_t off, mask; } FRHD[FG_DIM_N] = {
  {   0,  63 },   // FG_DIM_GENERO   cap 32 -> 64
  {  64, 127 },   // FG_DIM_PESSOA   cap 64 -> 128
  { 192, 255 },   // FG_DIM_TEMA     cap 96 -> 256
  { 448,  31 },   // FG_DIM_DECADA   cap 12 -> 32
  { 480,  31 },   // FG_DIM_IDIOMA   cap 16 -> 32
  { 512,  31 },   // FG_DIM_DURACAO  cap 10 -> 32
};

// O que o MMR recomputaria por par, pre-computado uma vez por candidato
// elegivel e guardado em vetores planos por posicao: genero principal,
// semente, franquia e a lista de pessoas ordenada/deduplicada — a
// interseccao vira um merge-scan de <= 11 passos em vez do laco 6x6, e as
// varreduras do MMR leem sequencial em vez de perseguir ponteiros.

static const FarolAfin *frVetor(const FarolGosto *g, int dim, int *cap) {
  switch (dim) {
    case FG_DIM_GENERO:  *cap = FG_GENERO_N;  return g->genero;
    case FG_DIM_PESSOA:  *cap = FG_PESSOA_N;  return g->pessoa;
    case FG_DIM_TEMA:    *cap = FG_TEMA_N;    return g->tema;
    case FG_DIM_DECADA:  *cap = FG_DECADA_N;  return g->decada;
    case FG_DIM_IDIOMA:  *cap = FG_IDIOMA_N;  return g->idioma;
    default:             *cap = FG_DURACAO_N; return g->duracao;
  }
}

// Espalhador dos ids nas tabelas: os bits ALTOS do produto de 64 bits
// (Fibonacci hashing) — os bits baixos do produto de 32 so dependem dos
// bits baixos do id, entao ids congruentes mod 64/128/256 se amontoavam no
// mesmo balde (pior caso medido na r2: multiplos de 4096, 0,40-0,58 ms).
// O bit usado no balde agora depende de todos os bits do id, por ~1
// operacao a mais por consulta, e nao muda a saida: a tabela compara o id
// inteiro e a sondagem sempre termina.
static uint32_t frEspalha(int32_t id) {
  return (uint32_t)(((uint64_t)(uint32_t)id * 0x9E3779B97F4A7C15ull) >> 32);
}

// Despeja as afinidades do modelo nas tabelas hash, com o peso ja projetado
// em `dia` (farolgosto_peso aplica o decaimento preguiçoso sem mexer no
// guardado). Id repetido dentro da dimensao fica com a 1.a copia — a mesma
// que a ordenacao estavel deixava primeiro na tabela antiga.
static void frMontaW(const FarolGosto *g, uint16_t dia, FRW *h) {
  int d, i;
  memset(h, 0, (size_t)FR_HSH_N * sizeof *h);
  for (d = 0; d < FG_DIM_N; d++) {
    const FarolAfin *arr;
    FRW *t = h + FRHD[d].off;
    uint32_t mask = FRHD[d].mask;
    int cap;
    arr = frVetor(g, d, &cap);
    for (i = 0; i < cap; i++) {
      uint32_t p;
      if (!arr[i].id) continue;
      p = frEspalha(arr[i].id) & mask;
      while (t[p].id && t[p].id != arr[i].id) p = (p + 1) & mask;
      if (t[p].id) continue;
      t[p].id = arr[i].id;
      t[p].n = arr[i].n;
      t[p].peso = (int16_t)farolgosto_peso(g, d, arr[i].id, dia);
    }
  }
}

static const FRW *frAcha(const FRW *h, int dim, int32_t id) {
  const FRW *t = h + FRHD[dim].off;
  uint32_t mask = FRHD[dim].mask;
  uint32_t p = frEspalha(id) & mask;
  while (t[p].id && t[p].id != id) p = (p + 1) & mask;
  return t[p].id == id ? &t[p] : 0;
}

static int16_t frPrende(int32_t v) {
  if (v > 256) return 256;
  if (v < -256) return -256;
  return (int16_t)v;
}

// Mesma tabela de faixas do farolgosto (durFaixa): <30, 30-60, 60-100,
// 100-140, >140 min. Duplicada de proposito — o modulo nao importa estatico
// de irmao; se a tabela mudar, muda nos dois (o teste confere a fronteira).
static int frDurFaixa(int min) {
  if (min <= 0) return -1;
  if (min < 30) return 0;
  if (min < 60) return 1;
  if (min < 100) return 2;
  if (min < 140) return 3;
  return 4;
}

// Media dos pesos projetados (Q8.8) nas afinidades `ids` da ficha, ids
// deduplicados; afinidade ausente do modelo conta 0. Cortada em [-1, +1].
static int32_t frTermo(const FRW *h, int dim, const int32_t *ids, int cap) {
  int64_t s = 0;
  int i, j, m = 0;
  for (i = 0; i < cap && ids[i]; i++) {
    const FRW *e;
    for (j = 0; j < i; j++) if (ids[j] == ids[i]) break;
    if (j < i) continue;
    e = frAcha(h, dim, ids[i]);
    if (e) s += e->peso;
    m++;
  }
  if (!m) return 0;
  return frPrende((int32_t)(s / m));
}

// Decada da ficha no mesmo id do modelo (1920 + idx*10, como farolgosto).
static int32_t frDecadaId(int ano) {
  int idx;
  if (ano <= 0) return 0;
  idx = (ano - 1920) / 10;
  if (idx < 0) idx = 0;
  if (idx > FG_DECADA_N - 1) idx = FG_DECADA_N - 1;
  return 1920 + idx * 10;
}

// Qualidade com votos (3.2): media bayesiana prior 6,8/200 votos -> [-1,+1].
static int32_t frQualidade(const FarolFicha *f) {
  int64_t m, v;
  if (!f->nota && !f->votos) return 0;
  v = (int64_t)f->votos + FR_QUAL_VOTOS;
  m = ((int64_t)f->nota * f->votos + (int64_t)FR_QUAL_PRIOR * FR_QUAL_VOTOS +
       v / 2) / v;                                   // 0..100
  return frPrende((int32_t)((m - 50) * 256 / 50));
}

// Bonus das fontes marcadas (tabela 3.1), em Q15. F_AMIGO so conta com
// afinidade >= 70 % — a regra da fonte, conferida aqui porque quem chama
// pode mandar duplicatas sem fundir (correcao FAROL41B).
static int32_t frBonus(const FarolRecCand *c) {
  int32_t b = 0;
  if (c->fontes & FRC_F_SEMENTE) b += FR_B_SEMENTE;
  if (c->fontes & FRC_F_PESSOA)  b += FR_B_PESSOA;
  if (c->fontes & FRC_F_SALVO)   b += FR_B_SALVO;
  if ((c->fontes & FRC_F_AMIGO) && c->gostoPct >= FR_AMIGO_MIN)
    b += FR_B_AMIGO;
  if (c->fontes & FRC_F_REC)     b += FR_B_REC;
  return b;
}

// Pontos brutos Q15 (3.2). Nivel 1 ("so genero e recs das sementes", 3.4)
// zera os outros termos — o bonus das fontes continua.
static int32_t frPontos(const FarolRecCtx *ctx, const FarolRecCand *c,
                        int nivel, const FRW *h) {
  const FarolFicha *f = c->ficha;
  int64_t acc;
  int32_t g1 = 0, tema = 0, pes = 0, dec = 0, idi = 0, hor = 0, qua = 0;
  if (f) {
    const FRW *e;
    g1 = frTermo(h, FG_DIM_GENERO, f->genero, FAROL_FICHA_GEN);
    if (nivel >= 2) {
      int fd;
      tema = frTermo(h, FG_DIM_TEMA, f->tema, FAROL_FICHA_TEMA);
      pes = frTermo(h, FG_DIM_PESSOA, f->pessoa, FAROL_FICHA_PESSOA);
      e = frAcha(h, FG_DIM_DECADA, frDecadaId(f->ano));
      dec = e ? e->peso : 0;
      e = frAcha(h, FG_DIM_IDIOMA, f->idioma);
      idi = e ? e->peso : 0;
      fd = frDurFaixa(f->minutos);
      if (fd >= 0)
        hor = frPrende(farolgosto_hora(ctx->g, ctx->faixaHora, fd, ctx->dia));
      qua = frQualidade(f);
    }
  }
  acc = (int64_t)FR_W_GENERO * g1 + (int64_t)FR_W_TEMA * tema +
        (int64_t)FR_W_PESSOA * pes + (int64_t)FR_W_DECADA * dec +
        (int64_t)FR_W_IDIOMA * idi + (int64_t)FR_W_HORA * hor +
        (int64_t)FR_W_QUAL * qua;
  acc = (acc + (acc >= 0 ? 128 : -128)) >> 8;      // Q23 -> Q15, arredondado
  acc += frBonus(c);
  if (c->h && farolgosto_mostrado_dias(ctx->g, c->h) >= 3)
    acc -= FR_P_MOSTR;
  if (acc > 0x7fffffffLL) acc = 0x7fffffffLL;
  if (acc < -0x7fffffffLL - 1) acc = -0x7fffffffLL - 1;
  return (int32_t)acc;
}

// Chave de desempate deterministica por (chave do titulo, semente do
// sorteio): so decide quando os pontos empatam — nunca muda quem passa.
static uint32_t frMistura(int64_t chave, uint32_t s) {
  uint64_t x = (uint64_t)chave * 0x9E3779B97F4A7C15ull ^
               (uint64_t)s * 0xD1B54A32D192ED03ull;
  x ^= x >> 33;
  x *= 0xFF51AFD7ED558CCDull;
  x ^= x >> 33;
  return (uint32_t)x;
}

// As duas listas de pessoas (ordenadas, sem repeticao) se cruzam? — o
// "pessoa em comum" da similaridade 3.3.
static int frTemPessoa(const int32_t *a, int na, const int32_t *b, int nb) {
  int i = 0, j = 0;
  while (i < na && j < nb) {
    if (a[i] == b[j]) return 1;
    if (a[i] < b[j]) i++; else j++;
  }
  return 0;
}

// A metade "0,5" da similaridade MMR (3.3): mesmo genero principal ou
// pessoa em comum — o mesmo resultado do laco antigo, so que sobre os
// auxiliares pre-computados.
static int frSimMeio(int32_t genA, const int32_t *pa, int na,
                     int32_t genB, const int32_t *pb, int nb) {
  return (genA && genA == genB) || frTemPessoa(pa, na, pb, nb);
}

// Regra dura da posicao `k` (0-based) para o elegivel `e`: no maximo 3 do
// mesmo genero principal entre os 6 primeiros, contados nos generos dos
// escolhidos ja registrados. As regras de semente e franquia NAO moram
// aqui: sao marcadas de uma vez na varredura que segue cada escolha (bit 1
// de st[]), em vez de recontadas por candidato a cada rodada — contagem
// identica, custo bem menor. A janela nao e ban permanente.
static int frPodeGen(int e, const int32_t *gen0, const int32_t *pGen,
                     int k) {
  int i, nGen = 0;
  if (k >= FR_JANELA_GEN || !gen0[e]) return 1;
  for (i = 0; i < k; i++)
    if (pGen[i] == gen0[e]) nGen++;
  return nGen < FR_MAX_GENERO6;
}

// Nota ajustada MMR com o simMax incremental mantido pela selecao.
static int64_t frAdj(int32_t pts, int32_t simM) {
  return (int64_t)FR_LAMBDA * pts - (int64_t)FR_UMENOSL * simM;
}

// n de evidencias do genero no modelo (0 se ausente) — o "n < 3" da vaga
// fora da curva.
static int frGenN(const FRW *h, int32_t id) {
  const FRW *e = frAcha(h, FG_DIM_GENERO, id);
  return e ? (int)e->n : 0;
}

int farolrec_nivel(const FarolGosto *g) {
  if (!g) return 0;
  if (g->titulos < 3) return 0;
  if (g->titulos < 10) return 1;
  return 2;
}

// Quantas afinidades as duas fichas dividem (genero+pessoa+tema, id a id) —
// o "2+" do FMO_TERMINOU.
static int frComum(const FarolFicha *a, const FarolFicha *b) {
  static const int CAP[3] = { FAROL_FICHA_GEN, FAROL_FICHA_PESSOA,
                              FAROL_FICHA_TEMA };
  int n = 0, d, i, j;
  for (d = 0; d < 3; d++) {
    const int32_t *va = d == 0 ? a->genero : d == 1 ? a->pessoa : a->tema;
    const int32_t *vb = d == 0 ? b->genero : d == 1 ? b->pessoa : b->tema;
    for (i = 0; i < CAP[d] && va[i]; i++)
      for (j = 0; j < CAP[d] && vb[j]; j++)
        if (va[i] == vb[j]) { n++; break; }
  }
  return n;
}

// Afinidade (dim,id) que sustenta o motivo de afinidade: slot com n >= 2 e
// peso projetado > 0,5 (regra de verdade 3.5). Devolve o mais forte; empate vai
// para pessoa, depois tema, depois genero — do mais especifico ao mais vago.
static int frMelhorDim(const FarolFicha *f, const FRW *h, int32_t *idOut) {
  static const struct { int dim, cap, tipo; } D[3] = {
    { FG_DIM_PESSOA, FAROL_FICHA_PESSOA, FMO_PESSOA },
    { FG_DIM_TEMA,   FAROL_FICHA_TEMA,   FMO_TEMA   },
    { FG_DIM_GENERO, FAROL_FICHA_GEN,    FMO_GENERO },
  };
  int d, i, melhor = -1;
  int16_t pesoM = 128;                        // > 0,5 em Q8.8
  for (d = 0; d < 3; d++) {
    const int32_t *ids = D[d].dim == FG_DIM_PESSOA ? f->pessoa :
                         D[d].dim == FG_DIM_TEMA   ? f->tema : f->genero;
    for (i = 0; i < D[d].cap && ids[i]; i++) {
      const FRW *e = frAcha(h, D[d].dim, ids[i]);
      if (e && e->n >= 2 && e->peso > pesoM) {
        pesoM = e->peso;
        melhor = d;
        *idOut = ids[i];
      }
    }
  }
  return melhor < 0 ? -1 : D[melhor].tipo;
}

// O id compartilhado mais forte (tema primeiro, depois genero) que tambem
// sustenta afinidade — o segundo argumento do FMO_TERMINOU na forma _TEMA.
static int32_t frComumForte(const FarolFicha *fc, const FarolFicha *fs,
                            const FRW *h) {
  static const struct { int dim, cap; } D[2] = {
    { FG_DIM_TEMA, FAROL_FICHA_TEMA }, { FG_DIM_GENERO, FAROL_FICHA_GEN },
  };
  int d, i, j;
  for (d = 0; d < 2; d++) {
    const int32_t *va = d == 0 ? fc->tema : fc->genero;
    const int32_t *vb = d == 0 ? fs->tema : fs->genero;
    for (i = 0; i < D[d].cap && va[i]; i++) {
      const FRW *e = frAcha(h, D[d].dim, va[i]);
      if (!e || e->n < 2 || e->peso <= 128) continue;
      for (j = 0; j < D[d].cap && vb[j]; j++)
        if (va[i] == vb[j]) return va[i];
    }
  }
  return 0;
}

// Ate 2 motivos por escolhido, so os que passam nas regras de verdade da
// 3.5. Ordem: a marca da exploracao ou a semente terminada; depois prova
// social (amigo/indicado), a afinidade mais forte, intencao (salvo) e o
// encaixe de duracao.
static void frMotivos(const FarolRecCtx *ctx, const FarolRecCand *cand,
                      FarolRecItem *it, int nivel, const FRW *h) {
  const FarolRecCand *c = &cand[it->cand];
  const FarolFicha *f = c->ficha;
  int n = 0;
  if (it->foraCurva) {
    it->motivo[n].tipo = FMO_FORA_CURVA;
    it->motivo[n].a = f ? f->genero[0] : 0;
    it->motivo[n].b = 0;
    n++;
  } else if (c->semente != FRC_SEM_SEMENTE && c->semente < ctx->nSementes) {
    const FarolRecSemente *s = &ctx->sementes[c->semente];
    if (s->terminou &&
        ((c->flags & FRC_FL_EM_REC) ||
         (f && s->ficha && frComum(f, s->ficha) >= 2))) {
      it->motivo[n].tipo = FMO_TERMINOU;
      it->motivo[n].a = c->semente;
      it->motivo[n].b = (nivel >= 2 && f && s->ficha)
                        ? frComumForte(f, s->ficha, h) : 0;
      n++;
    }
  }
  if (n < FAROL_REC_MOTIVOS && (c->fontes & FRC_F_AMIGO) && c->quem &&
      c->gostoPct >= FR_AMIGO_MIN) {
    it->motivo[n].tipo = FMO_AMIGO;
    it->motivo[n].a = c->quem;
    it->motivo[n].b = c->gostoPct;
    n++;
  }
  if (n < FAROL_REC_MOTIVOS && (c->fontes & FRC_F_REC) && c->quem) {
    it->motivo[n].tipo = FMO_RECOMENDADO;
    it->motivo[n].a = c->quem;
    it->motivo[n].b = 0;
    n++;
  }
  if (n < FAROL_REC_MOTIVOS && nivel >= 2 && f) {
    int32_t id = 0;
    int tipo = frMelhorDim(f, h, &id);
    if (tipo >= 0) {
      int32_t b = 0;
      // Segundo argumento: a semente apontada tambem tem a afinidade?
      // ("Do diretor de Duna" — b e o indice do titulo visto.)
      if (c->semente != FRC_SEM_SEMENTE && c->semente < ctx->nSementes &&
          ctx->sementes[c->semente].ficha) {
        const FarolFicha *fs = ctx->sementes[c->semente].ficha;
        const int32_t *vb = tipo == FMO_PESSOA ? fs->pessoa :
                            tipo == FMO_TEMA   ? fs->tema : fs->genero;
        int cap = tipo == FMO_PESSOA ? FAROL_FICHA_PESSOA :
                  tipo == FMO_TEMA   ? FAROL_FICHA_TEMA : FAROL_FICHA_GEN;
        int j;
        for (j = 0; j < cap && vb[j]; j++)
          if (vb[j] == id) { b = c->semente; break; }
      }
      it->motivo[n].tipo = (uint8_t)tipo;
      it->motivo[n].a = id;
      it->motivo[n].b = b;
      n++;
    }
  }
  if (n < FAROL_REC_MOTIVOS && (c->fontes & FRC_F_SALVO)) {
    it->motivo[n].tipo = FMO_SALVO;
    it->motivo[n].a = 0;
    it->motivo[n].b = 0;
    n++;
  }
  if (n < FAROL_REC_MOTIVOS && f && f->minutos > 0) {
    // "Cabe hoje" so se o modelo de hora nao e contra aquela duracao
    // agora — filme de 3 h as 23h nao cabe (correcao FAROL41B).
    int fd = frDurFaixa(f->minutos);
    if (fd >= 0 &&
        farolgosto_hora(ctx->g, ctx->faixaHora, fd, ctx->dia) >= 0) {
      it->motivo[n].tipo = FMO_CABE_HOJE;
      it->motivo[n].a = f->minutos;
      it->motivo[n].b = 0;
      n++;
    }
  }
  it->nMotivo = (uint8_t)n;
}

int farolrec_escolher(const FarolRecCtx *ctx,
                      const FarolRecCand *cand, int nCand,
                      FarolRecItem *out, int maxOut) {
  // Memoria de trabalho na pilha, 38 064 B medidos (-fstack-usage) no teto:
  // hsh 4,3 KB + vetores por posicao elegivel ~21,2 KB + ord/tmp/sl/st
  // ~3,6 KB + dedup setV/setF/emit 10 KB + escolhidos pSem/pGen ~0,9 KB.
  // Nada estatico: chamadas aninhadas nao se pisam.
  FRW hsh[FR_HSH_N];
  int32_t pts[FAROL_REC_CAND_MAX];      // por posicao elegivel
  int32_t simM[FAROL_REC_CAND_MAX];     // simMax incremental por posicao
  int32_t gen0[FAROL_REC_CAND_MAX];     // genero principal da ficha
  int32_t fran[FAROL_REC_CAND_MAX];     // franquia do candidato
  int32_t pes[FAROL_REC_CAND_MAX][FAROL_FICHA_PESSOA];  // pessoas ordenadas
  uint32_t hh[FAROL_REC_CAND_MAX];      // mistura(chave,semente) pronta
  uint16_t sem[FAROL_REC_CAND_MAX];     // semente do candidato
  uint16_t cix[FAROL_REC_CAND_MAX];     // indice do candidato na entrada
  uint16_t ord[FAROL_REC_CAND_MAX];     // permutacao de posicoes
  uint16_t tmp[FAROL_REC_CAND_MAX];
  uint16_t sl[FAROL_REC_CAND_MAX];      // slot dedup por indice de candidato
  uint8_t nPes[FAROL_REC_CAND_MAX];
  uint8_t st[FAROL_REC_CAND_MAX];       // bit0 ja escolhido, bit1 banido
  int64_t setV[FR_SET_N];               // dedup por chave (enderecamento aberto)
  uint8_t setF[FR_SET_N];               // flags unidas por chave (byte inteiro
                                        // do chamador — nenhum bit e' do motor)
  uint8_t emit[FR_SET_N];               // marca "ja emitida" por slot de dedup
                                        // — campo proprio: o bit 7 de `flags`
                                        // e' do chamador, nao colide (r2 E1l)
  uint16_t pSem[FAROL_REC_CAND_MAX];    // semente de cada escolhido
  int32_t pGen[FR_JANELA_GEN];          // genero principal dos <= 6 primeiros
  int nivel, nEl = 0, nOut = 0, foraPronto, i, k;

  if (!ctx || !ctx->g || !cand || !out || maxOut < 1 || nCand < 1) return 0;
  nivel = farolrec_nivel(ctx->g);
  if (!nivel) return 0;                    // frio 0: nenhuma fileira (3.4)
  if (nCand > FAROL_REC_CAND_MAX) nCand = FAROL_REC_CAND_MAX;
  frMontaW(ctx->g, ctx->dia, hsh);

  // --- dedup por chave ANTES dos filtros, unindo as flags das copias (3.1
  // + contrato 44/45): um titulo VISTO/fora da classificacao por uma via
  // nao volta pela copia "limpa". Quem chama continua fundindo `fontes` —
  // aqui so as flags de saida (VISTO, FORA_CLASS, CONTINUAR, FORA_CAT/
  // TEM_IMDB) viram uniao por chave.
  memset(setV, 0, sizeof setV);
  memset(setF, 0, sizeof setF);
  memset(emit, 0, sizeof emit);
  for (i = 0; i < nCand; i++) {
    uint32_t p;
    sl[i] = 0xffff;
    if (!cand[i].chave) continue;
    p = (uint32_t)(frMistura(cand[i].chave, 0) & (FR_SET_N - 1));
    while (setV[p] && setV[p] != cand[i].chave) p = (p + 1) & (FR_SET_N - 1);
    setV[p] = cand[i].chave;
    setF[p] |= cand[i].flags;
    sl[i] = (uint16_t)p;
  }

  // --- o que sai antes de pontuar (3.1), sobre as flags unidas ----------
  memset(st, 0, sizeof st);
  for (i = 0; i < nCand; i++) {
    const FarolRecCand *c = &cand[i];
    const FarolFicha *f = c->ficha;
    uint8_t fl = c->flags;
    int e = nEl, a2, b2;
    if (sl[i] != 0xffff) fl |= setF[sl[i]];
    if (!c->fontes) continue;
    if (fl & FRC_FL_VISTO) continue;
    if (fl & FRC_FL_FORA_CLASS) continue;
    if (ctx->naHome && (fl & FRC_FL_CONTINUAR)) continue;
    if ((fl & FRC_FL_FORA_CAT) && !(fl & FRC_FL_TEM_IMDB))
      continue;                            // aterramento (ALMA 6, risco 3)
    if (c->h && farolgosto_recusado(ctx->g, c->h)) continue;
    if (c->abandonoDia &&
        (int)ctx->dia - (int)c->abandonoDia < 90) continue;
    if (nivel == 1 && !(c->fontes & FRC_F_SEMENTE)) continue;
    if (sl[i] != 0xffff) {
      // A chave guarda a PRIMEIRA entrada que passou nos filtros.
      if (emit[sl[i]]) continue;
      emit[sl[i]] = 1;
    }
    // Achata por posicao elegivel tudo que as varreduras do MMR leem: o
    // resto do caminho nao volta a tocar o vetor de candidatos.
    ord[e] = (uint16_t)e;
    cix[e] = (uint16_t)i;
    pts[e] = frPontos(ctx, c, nivel, hsh);
    hh[e] = frMistura(c->chave, ctx->semente);
    sem[e] = c->semente;
    fran[e] = c->franquia;
    gen0[e] = f ? f->genero[0] : 0;
    nPes[e] = 0;
    if (f) {
      for (a2 = 0; a2 < FAROL_FICHA_PESSOA && f->pessoa[a2]; a2++) {
        for (b2 = 0; b2 < nPes[e] && pes[e][b2] != f->pessoa[a2]; b2++);
        if (b2 == nPes[e]) pes[e][nPes[e]++] = f->pessoa[a2];
      }
      for (a2 = 1; a2 < nPes[e]; a2++) {
        int32_t t = pes[e][a2];
        for (b2 = a2; b2 > 0 && pes[e][b2 - 1] > t; b2--)
          pes[e][b2] = pes[e][b2 - 1];
        pes[e][b2] = t;
      }
    }
    nEl++;
  }
  if (!nEl) return 0;

  // Ordena por (pontos desc, mistura(chave,semente) asc): merge sort
  // iterativo, deterministico e sem qsort_r.
  for (k = 1; k < nEl; k <<= 1) {
    int a;
    for (a = 0; a < nEl; a += 2 * k) {
      int b = a + k, e = a + 2 * k, p = a, q = b, t = a;
      if (b > nEl) b = nEl;
      if (e > nEl) e = nEl;
      while (p < b && q < e) {
        int x = ord[p], y = ord[q];
        int antes = pts[x] > pts[y] || (pts[x] == pts[y] && hh[x] < hh[y]);
        tmp[t++] = (uint16_t)(antes ? x : y);
        if (antes) p++; else q++;
      }
      while (p < b) tmp[t++] = ord[p++];
      while (q < e) tmp[t++] = ord[q++];
    }
    for (a = 0; a < nEl; a++) ord[a] = tmp[a];
  }
  foraPronto = (nivel < 2);                // fora da curva so no nivel 2
  memset(simM, 0, sizeof simM);

  // --- selecao gulosa MMR com regras duras (3.3) ------------------------
  // simM guarda a maior similaridade ja vista contra os escolhidos; cada
  // escolha so atualiza esse vetor — e quem ja esta em 1,0, ja saiu ou ja
  // esta banido nem entra na varredura.
  while (nOut < maxOut) {
    int pick = -1, fc = 0;
    int64_t best = -1;
    uint32_t bestH = 0;
    // Vaga "fora da curva": nas posicoes 5-8, antes do melhor normal, o
    // melhor candidato de genero com n < 3 no modelo (3.3).
    if (!foraPronto && nOut >= FR_FC_INI && nOut <= FR_FC_FIM) {
      for (i = 0; i < nEl; i++) {
        int e = ord[i];
        int64_t adj;
        if (st[e] & 3) continue;
        if (!gen0[e] || frGenN(hsh, gen0[e]) >= 3) continue;
        if (!frPodeGen(e, gen0, pGen, nOut)) continue;
        adj = frAdj(pts[e], simM[e]);
        if (pick < 0 || adj > best || (adj == best && hh[e] < bestH)) {
          pick = e; best = adj; bestH = hh[e];
        }
      }
      if (pick >= 0) fc = 1;
    }
    if (pick < 0) {
      for (i = 0; i < nEl; i++) {
        int e = ord[i];
        int64_t adj;
        if (st[e] & 3) continue;
        if (!frPodeGen(e, gen0, pGen, nOut)) continue;
        adj = frAdj(pts[e], simM[e]);
        if (pick < 0 || adj > best || (adj == best && hh[e] < bestH)) {
          pick = e; best = adj; bestH = hh[e];
        }
      }
    }
    if (pick < 0) break;                   // nada elegivel: fileira mais curta
    {
      int e = pick;
      FarolRecItem *it = &out[nOut];
      int32_t gJ = gen0[e], fJ = fran[e];
      uint16_t sJ = sem[e];
      memset(it, 0, sizeof *it);
      it->cand = cix[e];
      it->bruto = pts[e];
      it->pontos = (int32_t)(best / 32768);   // Q23 -> Q15
      it->foraCurva = (uint8_t)fc;
      st[e] |= 1;
      pSem[nOut] = sJ;
      if (nOut < FR_JANELA_GEN) pGen[nOut] = gJ;
      nOut++;
      if (fc) foraPronto = 1;
      {
        const int32_t *pJ = pes[e];
        int nJ = nPes[e], nSJ = 0;
        // As regras duras de semente (2 por fileira) e franquia (1) viram
        // marca de uma vez: quem divide franquia com o escolhido — ou a
        // semente que acabou de completar o teto — esta banido (bit 1).
        if (fJ || sJ != FRC_SEM_SEMENTE) {
          if (sJ != FRC_SEM_SEMENTE)
            for (i = 0; i < nOut; i++) if (pSem[i] == sJ) nSJ++;
          for (i = 0; i < nEl; i++)
            if ((fJ && fran[i] == fJ) ||
                (nSJ >= FR_MAX_SEMENTE && sem[i] == sJ))
              st[i] |= 2;
        }
        for (i = 0; i < nEl; i++) {
          int32_t s;
          if ((st[i] & 3) || simM[i] >= FR_SIM_MESMO) continue;
          // 1,0 so por semente/franquia; com simM ja em 0,5 a varredura de
          // genero e pessoas nao muda nada (0,5 nao sobe o que ja tem 0,5).
          if (sem[i] != FRC_SEM_SEMENTE && sem[i] == sJ)
            s = FR_SIM_MESMO;
          else if (fran[i] && fran[i] == fJ)
            s = FR_SIM_MESMO;
          else if (simM[i] < FR_SIM_MEIO &&
                   frSimMeio(gen0[i], pes[i], nPes[i], gJ, pJ, nJ))
            s = FR_SIM_MEIO;
          else
            s = 0;
          if (s > simM[i]) simM[i] = s;
        }
      }
    }
  }

  // --- motivos (3.5), ja com a fileira fechada ---------------------------
  for (i = 0; i < nOut; i++)
    frMotivos(ctx, cand, &out[i], nivel, hsh);
  return nOut;
}
