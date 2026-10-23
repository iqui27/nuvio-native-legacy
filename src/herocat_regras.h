// herocat_regras.h: regras puras do seletor "Catálogos do destaque" — quais
// catálogos entram em "Sugeridos" (Trending, Latest, Top, Anticipated; inglês
// e português) e o filtro da busca do modal. Sem estado, para o teste.
#ifndef HEROCAT_REGRAS_H
#define HEROCAT_REGRAS_H
#include "livetv_regras.h"

enum { HC_TRENDING = 0, HC_LATEST, HC_TOP, HC_ANTICIPATED };
#define HC_SUG_MAX 6

// Palavras inteiras (já dobradas) de cada classe.
static inline int hc_palavra_classe(const char *w) {
  static const char *const P[4][8] = {
    { "trending", "tendencias", "tendencia", "alta", NULL },
    { "latest", "new", "newest", "recent", "recentes", "lancamentos", "novidades", NULL },
    { "top", "popular", "populares", "vistos", NULL },
    { "anticipated", "upcoming", "breve", NULL },
  };
  int c, k;
  for (c = 0; c < 4; c++)
    for (k = 0; P[c][k]; k++) if (!strcmp(w, P[c][k])) return c;
  return -1;
}
// Classe do texto (palavras inteiras, dobrado); -1 se nenhuma. A menor vence.
static inline int hc_classe_texto(const char *txt) {
  char b[200], w[48];
  int melhor = -1, u = 0, i;
  nv_dobrar(txt, b, sizeof b);
  for (i = 0;; i++) {
    unsigned char c = (unsigned char)b[i];
    if (c && isalnum(c) && u + 1 < (int)sizeof w) { w[u++] = (char)c; continue; }
    if (u) {
      int k;
      w[u] = 0; u = 0;
      k = hc_palavra_classe(w);
      if (k >= 0 && (melhor < 0 || k < melhor)) melhor = k;
    }
    if (!c) break;
  }
  return melhor;
}
// Classe do catálogo pelo id ou pelo nome; -1 se nenhuma.
static inline int hc_classe(const char *chave, const char *titulo) {
  int a = hc_classe_texto(chave), b = hc_classe_texto(titulo);
  return a < 0 ? b : (b < 0 || a < b) ? a : b;
}
// Até `max` índices de sugeridos, por classe e depois pela ordem original.
// Pula o catálogo já escolhido (`atual`).
static inline int hc_sugeridos(int n, const char *const *chaves, const char *const *titulos,
                               const char *atual, int *out, int max) {
  int cls[512], c, i, m = 0;
  if (n > 512) n = 512;
  for (i = 0; i < n; i++) cls[i] = (atual && !strcmp(chaves[i], atual)) ? -1 : hc_classe(chaves[i], titulos[i]);
  for (c = 0; c < 4; c++)
    for (i = 0; i < n && m < max; i++) if (cls[i] == c) out[m++] = i;
  return m;
}
// A busca (cada palavra, sem acento nem caixa) aparece no nome ou no addon?
static inline int hc_filtro(const char *busca, const char *titulo, const char *addon) {
  char q[160], h[400], w[160];
  const char *p;
  nv_dobrar(busca, q, sizeof q);
  nv_dobrar(titulo, h, sizeof h / 2);
  strcat(h, " ");
  nv_dobrar(addon, h + strlen(h), sizeof h - strlen(h));
  for (p = q; *p;) {
    size_t l;
    while (*p == ' ') p++;
    l = strcspn(p, " ");
    if (!l) break;
    snprintf(w, sizeof w, "%.*s", (int)l, p);
    if (!strstr(h, w)) return 0;
    p += l;
  }
  return 1;
}
#endif
