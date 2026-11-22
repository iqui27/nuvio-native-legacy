#include "legtemp.h"
#include <math.h>
#include <string.h>
#include <strings.h>

void legtemp_zerar(LegTemp *t) { memset(t, 0, sizeof *t); }

static int trocar(LegTemp *t, int novos) {
  int antes = t->motivos;
  t->motivos = novos;
  if (!antes && novos) return LEGTEMP_LIGAR;
  if (antes && !novos) return LEGTEMP_DESLIGAR;
  return LEGTEMP_NADA;
}

int legtemp_mudo(LegTemp *t, int mudo) {
  if (mudo < 0) return LEGTEMP_NADA;
  mudo = mudo != 0;
  if (mudo == t->mudo) return LEGTEMP_NADA;
  t->mudo = mudo;
  return trocar(t, mudo ? t->motivos | LEGTEMP_MUDO : t->motivos & ~LEGTEMP_MUDO);
}

int legtemp_busca(LegTemp *t, double de, double para) {
  if (!isfinite(de) || !isfinite(para)) return LEGTEMP_NADA;
  if (t->motivos & LEGTEMP_VOLTA) {
    // Ja revendo: outro recuo soma ate o teto; para a frente do ponto, acabou.
    if (para >= t->voltaFim - LEGTEMP_VOLTA_FOLGA ||
        t->voltaFim - LEGTEMP_VOLTA_FOLGA - para > LEGTEMP_VOLTA_MAX)
      return trocar(t, t->motivos & ~LEGTEMP_VOLTA);
    return LEGTEMP_NADA;
  }
  if (de - para < LEGTEMP_VOLTA_MIN || de - para > LEGTEMP_VOLTA_MAX) return LEGTEMP_NADA;
  t->voltaFim = de + LEGTEMP_VOLTA_FOLGA;
  return trocar(t, t->motivos | LEGTEMP_VOLTA);
}

int legtemp_quadro(LegTemp *t, double pos) {
  if (!(t->motivos & LEGTEMP_VOLTA) || !isfinite(pos) || pos < t->voltaFim) return LEGTEMP_NADA;
  return trocar(t, t->motivos & ~LEGTEMP_VOLTA);
}

int legtemp_idiomas(const char *pref, const char *ui, const char *audio, const char *out[3]) {
  const char *v[3] = { pref, ui, audio };
  int n = 0;
  for (int i = 0; i < 3; i++) {
    const char *s = v[i];
    int j;
    if (!s || !s[0] || !strcasecmp(s, "none") || !strcasecmp(s, "und")) continue;
    for (j = 0; j < n && strcasecmp(out[j], s); j++) {}
    if (j == n) out[n++] = s;
  }
  return n;
}
