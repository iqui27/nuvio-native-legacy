// PERFIL E STATS DO DIARIO (diarioperfil.h).
#include "diarioperfil.h"
#include "diario.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  long long ini, fim;          // mes corrente [ini, fim)
  long long anoIni;
  int mesAtual[32];
  unsigned char *diasAno;      // 366 dias do ano
  unsigned char *diasVistos;   // 400 dias para tras (sequencia)
  long long hojeIni;
  PerfilDados *d;
  struct { char id[DIARIO_ID_MAX]; int plays, minutos; char det[24]; char tipo; } top[64];
  int nTop;
} Acum;

static long long inicioDoDia(long long t) {
  time_t x = (time_t)t; struct tm m;
  localtime_r(&x, &m);
  m.tm_hour = m.tm_min = m.tm_sec = 0; m.tm_isdst = -1;
  return (long long)mktime(&m);
}

static int minutosDe(const DiarioEntrada *e) {
  if (e->seg > 0) return (e->seg + 30) / 60;
  if (e->origem == DIARIO_ORIG_PLAYER && e->durSeg > 0) return (e->durSeg + 30) / 60;
  return 0;
}

static int visita(const DiarioEntrada *e, void *c) {
  Acum *a = c;
  long long dia = inicioDoDia(e->quando);
  long long dias = (a->hojeIni - dia) / 86400;
  if (dias >= 0 && dias < 400) a->diasVistos[dias] = 1;
  if (e->quando >= a->anoIni) {
    time_t x = (time_t)e->quando; struct tm m; localtime_r(&x, &m);
    if (m.tm_yday >= 0 && m.tm_yday < 366) a->diasAno[m.tm_yday] = 1;
  }
  if (e->quando >= a->ini && e->quando < a->fim) {
    time_t x = (time_t)e->quando; struct tm m; int i, k = -1, min = minutosDe(e);
    localtime_r(&x, &m);
    a->d->plays++;
    if (e->temporada || e->episodio) a->d->episodios++; else a->d->filmes++;
    a->d->minutos += min;
    if (m.tm_mday >= 1 && m.tm_mday <= PERFIL_MAX_DIAS) a->d->atividade[m.tm_mday - 1]++;
    for (i = 0; i < a->nTop; i++) if (!strcmp(a->top[i].id, e->id)) { k = i; break; }
    if (k < 0 && a->nTop < 64) {
      k = a->nTop++;
      memset(&a->top[k], 0, sizeof a->top[k]);
      snprintf(a->top[k].id, sizeof a->top[k].id, "%s", e->id);
      a->top[k].tipo = e->tipo;
    }
    if (k >= 0) {
      a->top[k].plays++; a->top[k].minutos += min;
      if (e->temporada || e->episodio) snprintf(a->top[k].det, sizeof a->top[k].det, "T%dE%d", e->temporada, e->episodio);
    }
  }
  return 1;
}

int diario_perfil_dados(int perfil, PerfilDados *d, long long agora) {
  static const char *meses[] = { "Janeiro","Fevereiro","Março","Abril","Maio","Junho","Julho",
                                 "Agosto","Setembro","Outubro","Novembro","Dezembro" };
  Acum a;
  DiarioFiltro f;
  time_t t = (time_t)agora;
  struct tm m, p;
  int i, j, diasNoMes;
  if (!d) return 0;
  memset(d, 0, sizeof *d);
  memset(&f, 0, sizeof f);
  f.semFlags = DIARIO_F_PARCIAL;
  if (diario_percorrer(perfil, &f, NULL, NULL) <= 0) return 0;
  memset(&a, 0, sizeof a);
  a.d = d;
  a.diasAno = calloc(366, 1); a.diasVistos = calloc(400, 1);
  if (!a.diasAno || !a.diasVistos) { free(a.diasAno); free(a.diasVistos); return 0; }
  localtime_r(&t, &m);
  p = m; p.tm_mday = 1; p.tm_hour = p.tm_min = p.tm_sec = 0; p.tm_isdst = -1;
  a.ini = (long long)mktime(&p);
  p = m; p.tm_mday = 1; p.tm_mon++; p.tm_hour = p.tm_min = p.tm_sec = 0; p.tm_isdst = -1;
  a.fim = (long long)mktime(&p);
  p = m; p.tm_mday = 1; p.tm_mon = 0; p.tm_hour = p.tm_min = p.tm_sec = 0; p.tm_isdst = -1;
  a.anoIni = (long long)mktime(&p);
  a.hojeIni = inicioDoDia(agora);
  diario_percorrer(perfil, &f, visita, &a);

  snprintf(d->periodo, sizeof d->periodo, "%s %d", i18n(meses[m.tm_mon]), m.tm_year + 1900);
  diasNoMes = (int)((a.fim - a.ini + 43200) / 86400);
  if (diasNoMes > PERFIL_MAX_DIAS) diasNoMes = PERFIL_MAX_DIAS;
  d->nDias = diasNoMes;
  { struct tm q = m; q.tm_mday = 1; q.tm_isdst = -1; mktime(&q); d->primeiroDiaSemana = q.tm_wday; }
  for (i = 0; i < d->nDias; i++) if (d->atividade[i]) d->diasAtivosMes++;
  for (i = 0; i < 366; i++) if (a.diasAno[i]) d->diasAtivosAno++;
  // Sequencia: hoje (ou ontem, se hoje ainda nao teve nada) para tras. O Diario
  // cobre a virada do mes, entao a sequencia e a de verdade.
  i = a.diasVistos[0] ? 0 : 1;
  for (; i < 400 && a.diasVistos[i]; i++) d->streakAtual++;
  d->streakCompleto = 1;
  // O ano so e "completo" se o diario comeca antes dele (senao e recorte).
  { DiarioEntrada prim; d->anoCompleto = diario_entrada(perfil, 0, &prim) && prim.quando < a.anoIni; }

  for (i = 0; i < a.nTop; i++) for (j = i + 1; j < a.nTop; j++)
    if (a.top[j].plays > a.top[i].plays) { __typeof__(a.top[0]) x = a.top[i]; a.top[i] = a.top[j]; a.top[j] = x; }
  for (i = 0; i < a.nTop && d->nDestaques < PERFIL_MAX_DESTAQUES; i++) {
    PerfilDestaque *h = &d->destaques[d->nDestaques];
    char titulo[160], poster[512];
    int ano;
    memset(h, 0, sizeof *h);
    if (!diario_nome(a.top[i].id, titulo, sizeof titulo, &ano, poster, sizeof poster)) {
      // Sem nome conhecido nao ha o que mostrar com honestidade (so com id).
      if (strncmp(a.top[i].id, "tt", 2)) continue;
      titulo[0] = 0;
    }
    snprintf(h->id, sizeof h->id, "%s", a.top[i].id);
    snprintf(h->titulo, sizeof h->titulo, "%s", titulo);
    snprintf(h->detalhe, sizeof h->detalhe, "%s", a.top[i].det[0] ? a.top[i].det : "Filme");
    if (poster[0]) snprintf(h->poster, sizeof h->poster, "%s", poster);
    else if (!strncmp(h->id, "tt", 2))
      snprintf(h->poster, sizeof h->poster, "https://images.metahub.space/poster/medium/%s/img", h->id);
    if (!strncmp(h->id, "tt", 2))
      snprintf(h->backdrop, sizeof h->backdrop, "https://images.metahub.space/background/medium/%s/img", h->id);
    h->plays = a.top[i].plays; h->minutos = a.top[i].minutos;
    if (!h->titulo[0]) continue;
    d->nDestaques++;
  }
  d->parcial = 0;
  snprintf(d->aviso, sizeof d->aviso, "%s",
           "Do seu Diário: o que você viu nesta TV e na sua conta. Os minutos contam só o que tocou no Nuvio.");
  free(a.diasAno); free(a.diasVistos);
  return 1;
}
