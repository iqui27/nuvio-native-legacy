// Creditos automaticos: somente intervalos com dois limites verificados.
#ifndef NV_CREDITOSAUTO_H
#define NV_CREDITOSAUTO_H
#include "intro.h"
#include <math.h>
#include <string.h>

typedef struct {
  IntroTrecho usados[8];
  int nUsados, voltou, fonteMudou, buscaPendente, buscaRecuo;
  double voltouDe, voltouAte, duracao, fimBusca, inicioBusca;
  char fonte[4096];
} CreditosAuto;

static inline void creditosauto_zerar(CreditosAuto *s) { memset(s, 0, sizeof *s); }

// Acompanha a fonte mesmo com a opcao desligada ou o video ainda carregando.
// Outro arquivo pode ter um corte diferente dos marcadores deste titulo.
static inline void creditosauto_fonte(CreditosAuto *s, const char *url) {
  size_t n;
  if (!url || !url[0]) return;
  n = strlen(url);
  if (n >= sizeof s->fonte) { s->fonteMudou = 1; s->buscaPendente = 0; return; }
  if (!s->fonte[0]) memcpy(s->fonte, url, n + 1);
  else if (strcmp(s->fonte, url)) { s->fonteMudou = 1; s->buscaPendente = 0; }
}

// Guardar o gesto mesmo sem marcadores: uma resposta atrasada nao pula o
// trecho que a pessoa acabou de escolher para rever.
static inline void creditosauto_recuar(CreditosAuto *s, double de, double para) {
  if (!isfinite(de) || !isfinite(para) || para >= de) return;
  if (!s->voltou || de > s->voltouDe) s->voltouDe = de;
  if (!s->voltou || para < s->voltouAte) s->voltouAte = para;
  s->voltou = 1;
  s->fimBusca = para; s->inicioBusca = de;
  s->buscaPendente = s->buscaRecuo = 1;
}

// video_pos pode anunciar o destino antes de o seek chegar ao pipeline.
// A contagem de Next espera progresso depois dele, ou o fim nativo da midia.
static inline int creditosauto_aguardar(CreditosAuto *s, double pos, int tocando, int terminou) {
  if (!s->buscaPendente) return 0;
  // No recuo, um fim/tempo antigo ainda pode chegar depois do gesto. So o
  // progresso no trecho anterior confirma a busca; o destino otimista nao.
  if ((!s->buscaRecuo && terminou) ||
      (tocando && isfinite(pos) && pos > s->fimBusca + 0.25 &&
       (!s->buscaRecuo || pos < s->inicioBusca))) s->buscaPendente = 0;
  return s->buscaPendente;
}

// A lista vem de intro_creditos_limitados, nunca do marcador manual, de
// estimativas de fim ou de capitulos que so informam o inicio.
static inline int creditosauto_decidir(CreditosAuto *s, const IntroTrecho *v,
                                       int n, double pos, double dur, int filme,
                                       int ligado, int elegivel, double *fim) {
  if (!ligado || !elegivel || !v || !fim || !isfinite(pos) || pos < 0.0 ||
      !isfinite(dur) || dur <= 1.0 || s->fonteMudou || s->buscaPendente) return 0;
  if (s->duracao <= 1.0) s->duracao = dur;
  else if (fabs(s->duracao - dur) > 2.0) { s->fonteMudou = 1; return 0; }
  for (int i = 0; i < n; i++) {
    int usado = 0;
    if (v[i].tipo != INTRO_CREDITOS || !isfinite(v[i].inicio) || !isfinite(v[i].fim) ||
        v[i].inicio < 0.0 || v[i].fim <= v[i].inicio || v[i].fim > dur ||
        v[i].fim - v[i].inicio > 900.0 || (filme && v[i].inicio < dur * 0.5) ||
        pos < v[i].inicio || pos >= v[i].fim) continue;
    if (s->voltou && s->voltouDe >= v[i].inicio && s->voltouAte < v[i].fim) continue;
    for (int j = 0; j < s->nUsados; j++)
      if (s->usados[j].inicio == v[i].inicio && s->usados[j].fim == v[i].fim) usado = 1;
    if (usado || s->nUsados >= 8) continue;
    s->usados[s->nUsados++] = v[i]; // antes do seek: atraso/falha nao repete o pedido
    *fim = v[i].fim;
    s->fimBusca = *fim;
    s->buscaPendente = 1; s->buscaRecuo = 0;
    return 1;
  }
  return 0;
}
#endif
