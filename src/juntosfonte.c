// Watch Together: a fonte de cada um — ver juntosfonte.h.
#include "juntosfonte.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DUR_TOL_MS   2000      // Syncplay avisa acima de 2,5 s; aqui 2 s (plano 5.2)
#define PAL_RAZAO    (25.0 / 23.976)

const char *jsf_nome(JsVeredito v) {
  switch (v) {
    case JSF_MESMA: return "mesma";
    case JSF_PARECIDA: return "parecida";
    case JSF_DIFERENTE: return "diferente";
    default: return "nenhuma";
  }
}

// So o nome do arquivo: sem pasta, e nunca algo que pareca URL.
static void baseArquivo(const char *a, char *dst, size_t n) {
  const char *b = a, *p;
  dst[0] = 0;
  if (!a || !*a || strstr(a, "://")) return;
  for (p = a; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
  snprintf(dst, n, "%s", b);
}

static int hashOk(const char *h) {
  int i;
  if (!h) return 0;
  for (i = 0; i < 40; i++) if (!isxdigit((unsigned char)h[i])) return 0;
  return h[40] == 0;
}

// "Dune.Part.Two.2024.1080p.WEB-DL.mkv" -> "dune part two 2024 1080p web dl mkv"
static void normalizar(const char *s, char *d, size_t n) {
  size_t k = 0;
  int esp = 1;
  for (; s && *s && k + 1 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c)) { d[k++] = (char)tolower(c); esp = 0; }
    else if (!esp) { d[k++] = ' '; esp = 1; }
  }
  while (k && d[k - 1] == ' ') k--;
  d[k] = 0;
}

// Parecenca de nomes: fracao de palavras (>= 2 letras) do menor que existem no outro.
static double parecenca(const char *a, const char *b) {
  char na[256], nb[256], *t, *ctx = NULL;
  int total = 0, casou = 0;
  normalizar(a, na, sizeof na);
  normalizar(b, nb, sizeof nb);
  if (!na[0] || !nb[0]) return 0;
  if (strlen(na) > strlen(nb)) { char x[256]; memcpy(x, na, sizeof x); memcpy(na, nb, sizeof na); memcpy(nb, x, sizeof nb); }
  for (t = strtok_r(na, " ", &ctx); t; t = strtok_r(NULL, " ", &ctx)) {
    char pad[260];
    if (strlen(t) < 2) continue;
    total++;
    snprintf(pad, sizeof pad, " %s ", t);
    { char nbe[260]; snprintf(nbe, sizeof nbe, " %s ", nb); if (strstr(nbe, pad)) casou++; }
  }
  return total ? (double)casou / total : 0;
}

void jsf_rotulo(const Stream *s, char *dst, size_t n) {
  static const char *tags[][2] = {
    { "remux", "REMUX" }, { "bluray", "BluRay" }, { "blu ray", "BluRay" }, { "bdrip", "BDRip" },
    { "web dl", "WEB-DL" }, { "webdl", "WEB-DL" }, { "webrip", "WEBRip" }, { "hdtv", "HDTV" },
  };
  char nome[600], alvo[1200];
  const char *tag = "";
  size_t i;
  snprintf(alvo, sizeof alvo, "%s %s", s->arquivo, s->rotulo);
  normalizar(alvo, nome, sizeof nome);
  for (i = 0; i < sizeof tags / sizeof tags[0]; i++) {
    char pad[24];
    snprintf(pad, sizeof pad, " %s ", tags[i][0]);
    { char e[620]; snprintf(e, sizeof e, " %s ", nome); if (strstr(e, pad)) { tag = tags[i][1]; break; } }
  }
  if (s->altura > 0) snprintf(dst, n, "%dp%s%s", s->altura, *tag ? " " : "", tag);
  else snprintf(dst, n, "%s", tag);
}

void jsf_impressao(const Stream *s, double durS, JsImpressao *o) {
  memset(o, 0, sizeof *o);
  o->idx = -1;
  if (hashOk(s->infoHash)) {
    int i;
    for (i = 0; i < 40; i++) o->hash[i] = (char)tolower((unsigned char)s->infoHash[i]);
    o->hash[40] = 0;
    if (s->fileIdx >= 0) o->idx = s->fileIdx;
  }
  o->bytes = (long long)s->tamanhoBytes;
  baseArquivo(s->arquivo, o->arquivo, sizeof o->arquivo);
  if (!strstr(s->bingeGroup, "://")) snprintf(o->binge, sizeof o->binge, "%s", s->bingeGroup);
  o->altura = s->altura;
  o->durMs = durS > 0 ? (long long)llround(durS * 1000.0) : 0;
  jsf_rotulo(s, o->rotulo, sizeof o->rotulo);
}

static int mesmaPorHash(const JsImpressao *a, const Stream *s) {
  if (!a->hash[0] || !hashOk(s->infoHash)) return 0;
  if (strncasecmp(a->hash, s->infoHash, 40) != 0) return 0;
  // Pacote de temporada: o arquivo dentro do torrent tem de ser o mesmo. Um
  // lado sem fileIdx (addon que nao diz) ainda conta: o hash ja e do release.
  return a->idx < 0 || s->fileIdx < 0 || a->idx == s->fileIdx;
}

static int mesmaPorArquivo(const JsImpressao *a, const Stream *s) {
  char b[121], na[256], nb[256];
  if (a->bytes <= 0 || (long long)s->tamanhoBytes != a->bytes) return 0;
  baseArquivo(s->arquivo, b, sizeof b);
  normalizar(a->arquivo, na, sizeof na);
  normalizar(b, nb, sizeof nb);
  // Tamanho exato ja e forte; com o nome, e o mesmo arquivo. Sem nome dos dois
  // lados, um tamanho exato acima de 50 MB ainda vale (addon que nao manda o nome).
  if (na[0] && nb[0]) return strcmp(na, nb) == 0;
  return a->bytes > 50LL * 1024 * 1024;
}

JsVeredito jsf_casar(const JsImpressao *a, const Stream *l, int n, const unsigned char *ign, int *indice) {
  int i, melhor = -1, perto = -1, distPerto = 1 << 30;
  double melhorPar = 0;
  *indice = -1;
  if (!l || n <= 0) return JSF_NENHUMA;
  for (i = 0; i < n; i++) if (!(ign && ign[i]) && mesmaPorHash(a, &l[i])) { *indice = i; return JSF_MESMA; }
  for (i = 0; i < n; i++) if (!(ign && ign[i]) && mesmaPorArquivo(a, &l[i])) { *indice = i; return JSF_MESMA; }
  // Mesmo addon (bingeGroup) e mesma altura: o nome mais parecido.
  if (a->binge[0])
    for (i = 0; i < n; i++) {
      double p;
      if ((ign && ign[i]) || strcmp(a->binge, l[i].bingeGroup) || (a->altura && l[i].altura != a->altura)) continue;
      p = a->arquivo[0] ? parecenca(a->arquivo, l[i].arquivo) : 0.5;
      if (p > melhorPar) { melhorPar = p; melhor = i; }
    }
  if (melhor >= 0 && melhorPar >= 0.5) { *indice = melhor; return JSF_PARECIDA; }
  // Reserva: a primeira da lista (a ordem ja e a do automatico/addons) na
  // altura do anfitriao, ou a de altura mais perto.
  for (i = 0; i < n; i++) {
    int d;
    if (ign && ign[i]) continue;
    d = a->altura > 0 && l[i].altura > 0 ? abs(l[i].altura - a->altura) : 100000;
    if (d < distPerto) { distPerto = d; perto = i; }
    if (d == 0) break;
  }
  if (perto < 0) return JSF_NENHUMA;
  *indice = perto;
  return JSF_PARECIDA;
}

JsVeredito jsf_duracao(JsVeredito v, long long salaMs, double minhaS, int *pal) {
  double minhaMs = minhaS * 1000.0, r;
  if (pal) *pal = 0;
  if (salaMs <= 0 || minhaS <= 0 || v == JSF_NENHUMA) return v;
  if (minhaMs < 0.5 * (double)salaMs) return JSF_NENHUMA;   // clipe de aviso de addon
  if (fabs(minhaMs - (double)salaMs) <= DUR_TOL_MS) return v;
  r = (double)salaMs > minhaMs ? (double)salaMs / minhaMs : minhaMs / (double)salaMs;
  if (pal && fabs(r - PAL_RAZAO) < 0.003) *pal = 1;
  return JSF_DIFERENTE;
}
