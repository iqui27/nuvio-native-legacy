#include "aovivo.h"
#include "xtepg.h"
#include "xtream.h"
#include <stdio.h>
#include <string.h>

// --- zapping ------------------------------------------------------------------
void aovivo_zap_apertar(AoVivoZap *z, int dir, Uint32 agora) {
  if (!z || !dir) return;
  z->pend += dir > 0 ? 1 : -1;
  if (z->pend > AV_ZAP_MAX) z->pend = AV_ZAP_MAX;
  if (z->pend < -AV_ZAP_MAX) z->pend = -AV_ZAP_MAX;
  z->ultimo = agora;
}

int aovivo_zap_pronto(AoVivoZap *z, Uint32 agora, int *offset) {
  if (!z || !z->pend) return 0;
  if ((Uint32)(agora - z->ultimo) < AV_ZAP_MS) return 0;
  if (offset) *offset = z->pend;
  z->pend = 0;
  return 1;
}

int aovivo_ordem(int n, int atual, int offset) {
  long v;
  if (n < 1) return -1;
  if (atual < 0 || atual >= n) atual = 0;
  v = ((long)atual + offset) % n;
  if (v < 0) v += n;
  return (int)v;
}

// --- agora / a seguir ----------------------------------------------------------
void aovivo_selecionar(time_t t, const EpgProg *l, int n, int *iAg, int *iPx) {
  int i, ag = -1, px = -1;
  for (i = 0; i < n; i++)
    if (l[i].ini <= t && t < l[i].fim) ag = i;   // o mais recente que cobre
  if (ag >= 0) {
    for (i = ag + 1; i < n; i++)
      if (l[i].ini >= l[ag].fim) { px = i; break; }
  } else {
    for (i = 0; i < n; i++)
      if (l[i].ini > t) { px = i; break; }
  }
  if (iAg) *iAg = ag;
  if (iPx) *iPx = px;
}

#define AV_JANELA_N 24
int aovivo_epg_montar(int epgIdx, const char *xtId, time_t t, AoVivoEpg *o) {
  EpgProg l[AV_JANELA_N];
  int n = 0, ag, px;
  if (!o) return 0;
  memset(o, 0, sizeof *o);
  if (epgIdx >= 0) n = epg_faixa(epgIdx, t - 4 * 3600, t + 12 * 3600, l, AV_JANELA_N);
  else if (xtId && xtId[0] && xtream_e_id(xtId))
    n = xtepg_faixa(xtId, t - 4 * 3600, t + 12 * 3600, l, AV_JANELA_N);
  if (n > AV_JANELA_N) n = AV_JANELA_N;
  if (n < 1) return 0;
  aovivo_selecionar(t, l, n, &ag, &px);
  if (ag >= 0) {
    o->temAgora = 1;
    snprintf(o->agoraTit, sizeof o->agoraTit, "%s", l[ag].titulo ? l[ag].titulo : "");
    o->agoraIni = l[ag].ini; o->agoraFim = l[ag].fim;
    if (l[ag].fim > l[ag].ini)
      o->progresso = (float)(t - l[ag].ini) / (float)(l[ag].fim - l[ag].ini);
    if (o->progresso < 0.0f) o->progresso = 0.0f;
    if (o->progresso > 1.0f) o->progresso = 1.0f;
  }
  if (px >= 0) {
    o->temProx = 1;
    snprintf(o->proxTit, sizeof o->proxTit, "%s", l[px].titulo ? l[px].titulo : "");
    o->proxIni = l[px].ini;
  }
  return o->temAgora || o->temProx;
}


