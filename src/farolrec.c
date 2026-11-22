// Motor de recomendacao do Farol — ver farolrec.h (lote sense, issue 41;
// secao 3 do PLANO-GOSTO). Modulo puro: sem SDL, sem rede, sem malloc e sem
// relogio — `dia`/`faixaHora` vem no contexto e o sorteio vem de
// `ctx->semente`. Toda a memoria de trabalho fica na pilha da chamada:
// tabela de pesos projetados (~4,6 KB) + vetores de 400 (~3 KB).

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

// Peso projetado de uma afinidade, copiado a uma tabela por dimensao no
// inicio da chamada: depois disso cada consulta e busca binaria, sem varrer
// o vetor esparso do modelo por candidato (meta de tempo da 3.2).
typedef struct { int32_t id; int16_t peso; uint16_t n; } FRW;

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

// Monta w[dim][0..n) ordenado por id, com o peso ja projetado em `dia`
// (farolgosto_peso aplica o decaimento preguiçoso sem mexer no guardado).
// Devolve wn[dim]; w tem de ter FG_TEMA_N (o maior cap) por dimensao.
static void frMontaW(const FarolGosto *g, uint16_t dia,
                     FRW w[FG_DIM_N][FG_TEMA_N], int wn[FG_DIM_N]) {
  int d, i, j;
  for (d = 0; d < FG_DIM_N; d++) {
    const FarolAfin *arr;
    int cap, n = 0;
    arr = frVetor(g, d, &cap);
    for (i = 0; i < cap; i++) {
      if (!arr[i].id) continue;
      w[d][n].id = arr[i].id;
      w[d][n].n = arr[i].n;
      w[d][n].peso = (int16_t)farolgosto_peso(g, d, arr[i].id, dia);
      n++;
    }
    // Insertion sort: n <= 96 e quase ordenado na pratica.
    for (i = 1; i < n; i++) {
      FRW t = w[d][i];
      for (j = i; j > 0 && w[d][j - 1].id > t.id; j--) w[d][j] = w[d][j - 1];
      w[d][j] = t;
    }
    wn[d] = n;
  }
}

static const FRW *frAcha(const FRW *w, int n, int32_t id) {
  int a = 0, b = n;
  while (a < b) {
    int m = a + ((b - a) >> 1);
    if (w[m].id < id) a = m + 1; else b = m;
  }
  return (a < n && w[a].id == id) ? &w[a] : 0;
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
static int32_t frTermo(const FRW *w, int n, const int32_t *ids, int cap) {
  int64_t s = 0;
  int i, j, m = 0;
  for (i = 0; i < cap && ids[i]; i++) {
    const FRW *e;
    for (j = 0; j < i; j++) if (ids[j] == ids[i]) break;
    if (j < i) continue;
    e = frAcha(w, n, ids[i]);
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

// Bonus das fontes marcadas (tabela 3.1), em Q15.
static int32_t frBonus(uint8_t fontes) {
  int32_t b = 0;
  if (fontes & FRC_F_SEMENTE) b += FR_B_SEMENTE;
  if (fontes & FRC_F_PESSOA)  b += FR_B_PESSOA;
  if (fontes & FRC_F_SALVO)   b += FR_B_SALVO;
  if (fontes & FRC_F_AMIGO)   b += FR_B_AMIGO;
  if (fontes & FRC_F_REC)     b += FR_B_REC;
  return b;
}

// Pontos brutos Q15 (3.2). Nivel 1 ("so genero e recs das sementes", 3.4)
// zera os outros termos — o bonus das fontes continua.
static int32_t frPontos(const FarolRecCtx *ctx, const FarolRecCand *c,
                        int nivel, FRW w[FG_DIM_N][FG_TEMA_N],
                        const int wn[FG_DIM_N]) {
  const FarolFicha *f = c->ficha;
  int64_t acc;
  int32_t g1 = 0, tema = 0, pes = 0, dec = 0, idi = 0, hor = 0, qua = 0;
  if (f) {
    const FRW *e;
    g1 = frTermo(w[FG_DIM_GENERO], wn[FG_DIM_GENERO], f->genero,
                 FAROL_FICHA_GEN);
    if (nivel >= 2) {
      int fd;
      tema = frTermo(w[FG_DIM_TEMA], wn[FG_DIM_TEMA], f->tema,
                     FAROL_FICHA_TEMA);
      pes = frTermo(w[FG_DIM_PESSOA], wn[FG_DIM_PESSOA], f->pessoa,
                    FAROL_FICHA_PESSOA);
      e = frAcha(w[FG_DIM_DECADA], wn[FG_DIM_DECADA], frDecadaId(f->ano));
      dec = e ? e->peso : 0;
      e = frAcha(w[FG_DIM_IDIOMA], wn[FG_DIM_IDIOMA], f->idioma);
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
  acc += frBonus(c->fontes);
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

// Similaridade MMR entre dois candidatos (3.3), em Q15.
static int32_t frSim(const FarolRecCand *a, const FarolRecCand *b) {
  if (a->semente != FRC_SEM_SEMENTE && a->semente == b->semente)
    return FR_SIM_MESMO;
  if (a->franquia && a->franquia == b->franquia) return FR_SIM_MESMO;
  if (a->ficha && b->ficha) {
    int i, j;
    if (a->ficha->genero[0] && a->ficha->genero[0] == b->ficha->genero[0])
      return FR_SIM_MEIO;
    for (i = 0; i < FAROL_FICHA_PESSOA && a->ficha->pessoa[i]; i++)
      for (j = 0; j < FAROL_FICHA_PESSOA && b->ficha->pessoa[j]; j++)
        if (a->ficha->pessoa[i] == b->ficha->pessoa[j]) return FR_SIM_MEIO;
  }
  return 0;
}

// Regra dura da posicao `k` (0-based) para o candidato `c`, dado o que ja
// saiu em out[0..k). Devolve 1 se `c` pode entrar agora. semente/franquia
// estouradas tambem marcam *ban para nao recontar a cada rodada.
static int frPode(const FarolRecCand *cand, const FarolRecCand *c,
                  const FarolRecItem *out, int k, uint8_t *ban) {
  int i, nSem = 0, nGen = 0;
  if (*ban) return 0;
  if (c->franquia)
    for (i = 0; i < k; i++)
      if (cand[out[i].cand].franquia == c->franquia) { *ban = 1; return 0; }
  if (c->semente != FRC_SEM_SEMENTE) {
    for (i = 0; i < k; i++)
      if (cand[out[i].cand].semente == c->semente) nSem++;
    if (nSem >= FR_MAX_SEMENTE) { *ban = 1; return 0; }
  }
  if (k < FR_JANELA_GEN && c->ficha && c->ficha->genero[0]) {
    for (i = 0; i < k; i++) {
      const FarolFicha *o = cand[out[i].cand].ficha;
      if (o && o->genero[0] == c->ficha->genero[0]) nGen++;
    }
    if (nGen >= FR_MAX_GENERO6) return 0;   // so a janela: sem ban permanente
  }
  return 1;
}

// Nota ajustada MMR com o simMax incremental mantido pela selecao.
static int64_t frAdj(int32_t pts, int32_t simMax) {
  return (int64_t)FR_LAMBDA * pts - (int64_t)FR_UMENOSL * simMax;
}

// n de evidencias do genero no modelo (0 se ausente) — o "n < 3" da vaga
// fora da curva.
static int frGenN(const FRW *w, int n, int32_t id) {
  const FRW *e = frAcha(w, n, id);
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
static int frMelhorDim(const FarolFicha *f, FRW w[FG_DIM_N][FG_TEMA_N],
                       const int wn[FG_DIM_N], int32_t *idOut) {
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
      const FRW *e = frAcha(w[D[d].dim], wn[D[d].dim], ids[i]);
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
                            FRW w[FG_DIM_N][FG_TEMA_N],
                            const int wn[FG_DIM_N]) {
  static const struct { int dim, cap; } D[2] = {
    { FG_DIM_TEMA, FAROL_FICHA_TEMA }, { FG_DIM_GENERO, FAROL_FICHA_GEN },
  };
  int d, i, j;
  for (d = 0; d < 2; d++) {
    const int32_t *va = d == 0 ? fc->tema : fc->genero;
    const int32_t *vb = d == 0 ? fs->tema : fs->genero;
    for (i = 0; i < D[d].cap && va[i]; i++) {
      const FRW *e = frAcha(w[D[d].dim], wn[D[d].dim], va[i]);
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
                      FarolRecItem *it, int nivel,
                      FRW w[FG_DIM_N][FG_TEMA_N], const int wn[FG_DIM_N]) {
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
                        ? frComumForte(f, s->ficha, w, wn) : 0;
      n++;
    }
  }
  if (n < FAROL_REC_MOTIVOS && (c->fontes & FRC_F_AMIGO) && c->quem) {
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
    int tipo = frMelhorDim(f, w, wn, &id);
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
    it->motivo[n].tipo = FMO_CABE_HOJE;
    it->motivo[n].a = f->minutos;
    it->motivo[n].b = 0;
    n++;
  }
  it->nMotivo = (uint8_t)n;
}

int farolrec_escolher(const FarolRecCtx *ctx,
                      const FarolRecCand *cand, int nCand,
                      FarolRecItem *out, int maxOut) {
  // Memoria de trabalho na pilha: ~8 KB no teto (tabela de pesos + vetores
  // de 400 candidatos). Nada estatico: chamadas aninhadas nao se pisam.
  FRW w[FG_DIM_N][FG_TEMA_N];
  int wn[FG_DIM_N];
  int32_t pts[FAROL_REC_CAND_MAX];
  int32_t simM[FAROL_REC_CAND_MAX];     // simMax incremental por candidato
  uint16_t ord[FAROL_REC_CAND_MAX];
  uint16_t tmp[FAROL_REC_CAND_MAX];
  uint8_t ban[FAROL_REC_CAND_MAX];
  uint8_t sai[FAROL_REC_CAND_MAX];
  int64_t setV[FR_SET_N];               // dedup por chave (enderecamento aberto)
  uint8_t setU[FR_SET_N];
  int nivel, nEl = 0, nOut = 0, foraPronto, i, k;

  if (!ctx || !ctx->g || !cand || !out || maxOut < 1 || nCand < 1) return 0;
  nivel = farolrec_nivel(ctx->g);
  if (!nivel) return 0;                    // frio 0: nenhuma fileira (3.4)
  if (nCand > FAROL_REC_CAND_MAX) nCand = FAROL_REC_CAND_MAX;
  frMontaW(ctx->g, ctx->dia, w, wn);

  // --- o que sai antes de pontuar (3.1), com dedup pela chave -----------
  memset(ban, 0, sizeof ban);
  memset(sai, 0, sizeof sai);
  memset(setU, 0, sizeof setU);
  for (i = 0; i < nCand; i++) {
    const FarolRecCand *c = &cand[i];
    if (!c->fontes) continue;
    if (c->flags & FRC_FL_VISTO) continue;
    if (c->flags & FRC_FL_FORA_CLASS) continue;
    if (ctx->naHome && (c->flags & FRC_FL_CONTINUAR)) continue;
    if ((c->flags & FRC_FL_FORA_CAT) && !(c->flags & FRC_FL_TEM_IMDB))
      continue;                            // aterramento (ALMA 6, risco 3)
    if (c->h && farolgosto_recusado(ctx->g, c->h)) continue;
    if (c->abandonoDia &&
        (int)ctx->dia - (int)c->abandonoDia < 90) continue;
    if (nivel == 1 && !(c->fontes & FRC_F_SEMENTE)) continue;
    if (c->chave) {
      // Dedup por sondagem linear: a carga fica <= 400/1024, entao o laco
      // sempre encontra um slot vazio. Duplicata guarda a PRIMEIRA entrada.
      uint32_t p = (uint32_t)(frMistura(c->chave, 0) & (FR_SET_N - 1));
      int achou = 0;
      while (setU[p]) {
        if (setV[p] == c->chave) { achou = 1; break; }
        p = (p + 1) & (FR_SET_N - 1);
      }
      if (achou) continue;
      setU[p] = 1;
      setV[p] = c->chave;
    }
    ord[nEl] = (uint16_t)i;
    pts[i] = frPontos(ctx, c, nivel, w, wn);   // pts por indice de candidato
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
        int antes = pts[x] > pts[y] ||
                    (pts[x] == pts[y] &&
                     frMistura(cand[x].chave, ctx->semente) <
                     frMistura(cand[y].chave, ctx->semente));
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
  // escolha so atualiza esse vetor — nao recalcula sim contra todo mundo.
  while (nOut < maxOut) {
    int pick = -1, fc = 0;
    int64_t best = -1;
    uint32_t bestH = 0;
    // Vaga "fora da curva": nas posicoes 5-8, antes do melhor normal, o
    // melhor candidato de genero com n < 3 no modelo (3.3).
    if (!foraPronto && nOut >= FR_FC_INI && nOut <= FR_FC_FIM) {
      for (i = 0; i < nEl; i++) {
        const FarolRecCand *c;
        int64_t adj;
        uint32_t hh;
        if (sai[i]) continue;
        c = &cand[ord[i]];
        if (!c->ficha || !c->ficha->genero[0]) continue;
        if (frGenN(w[FG_DIM_GENERO], wn[FG_DIM_GENERO],
                   c->ficha->genero[0]) >= 3) continue;
        if (!frPode(cand, c, out, nOut, &ban[i])) continue;
        adj = frAdj(pts[ord[i]], simM[ord[i]]);
        hh = frMistura(c->chave, ctx->semente);
        if (pick < 0 || adj > best || (adj == best && hh < bestH)) {
          pick = i; best = adj; bestH = hh;
        }
      }
      if (pick >= 0) fc = 1;
    }
    if (pick < 0) {
      for (i = 0; i < nEl; i++) {
        const FarolRecCand *c;
        int64_t adj;
        uint32_t hh;
        if (sai[i]) continue;
        c = &cand[ord[i]];
        if (!frPode(cand, c, out, nOut, &ban[i])) continue;
        adj = frAdj(pts[ord[i]], simM[ord[i]]);
        hh = frMistura(c->chave, ctx->semente);
        if (pick < 0 || adj > best || (adj == best && hh < bestH)) {
          pick = i; best = adj; bestH = hh;
        }
      }
    }
    if (pick < 0) break;                   // nada elegivel: fileira mais curta
    {
      FarolRecItem *it = &out[nOut];
      const FarolRecCand *novo = &cand[ord[pick]];
      memset(it, 0, sizeof *it);
      it->cand = ord[pick];
      it->bruto = pts[ord[pick]];
      it->pontos = (int32_t)(best / 32768);   // Q23 -> Q15
      it->foraCurva = (uint8_t)fc;
      sai[pick] = 1;
      nOut++;
      if (fc) foraPronto = 1;
      for (i = 0; i < nEl; i++) {
        int32_t s;
        if (sai[i]) continue;
        s = frSim(&cand[ord[i]], novo);
        if (s > simM[ord[i]]) simM[ord[i]] = s;
      }
    }
  }

  // --- motivos (3.5), ja com a fileira fechada ---------------------------
  for (i = 0; i < nOut; i++)
    frMotivos(ctx, cand, &out[i], nivel, w, wn);
  return nOut;
}
