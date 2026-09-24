#include "artetamanho.h"
#include <stdio.h>
#include <string.h>

// `original` nao tem largura fixa (3840 no backdrop, 2000 no cartaz, 1920 ou
// 3840 no still do metahub): conta como maior que qualquer variante.
#define ORIGINAL 1000000

// "original" -> ORIGINAL, "wNNN" -> NNN, qualquer outro (hNNN do perfil,
// token desconhecido) -> 0, que quer dizer "nao mexer".
static int larguraDoToken(const char *t, size_t n) {
  size_t i;
  int v = 0;
  if (n == 8 && !strncmp(t, "original", 8)) return ORIGINAL;
  if (n < 2 || n > 6 || t[0] != 'w') return 0;
  for (i = 1; i < n; i++) {
    if (t[i] < '0' || t[i] > '9') return 0;
    v = v * 10 + (t[i] - '0');
  }
  return v;
}

// A menor da escada que cobre o teto, se ela for ao menos 1,5x menor que a
// atual. 0 = fica como esta.
static int escolher(const int *escada, int n, int atual, int limite) {
  int i;
  for (i = 0; i < n; i++)
    if (escada[i] >= limite)
      return atual >= escada[i] + escada[i] / 2 ? escada[i] : 0;
  return 0;
}

static int trocar(const char *url, const char *tok, size_t nTok,
                  const char *novo, char *saida, size_t tam) {
  int r = snprintf(saida, tam, "%.*s%s%s", (int)(tok - url), url, novo, tok + nTok);
  return r > 0 && (size_t)r < tam;
}

// O host tem de vir logo depois do esquema: "algo.com/?u=https://image.tmdb..."
// nao e do TMDB.
static const char *depoisDoHost(const char *url, const char *host) {
  const char *p;
  if (!strncmp(url, "https://", 8)) p = url + 8;
  else if (!strncmp(url, "http://", 7)) p = url + 7;
  else return NULL;
  return strncmp(p, host, strlen(host)) ? NULL : p + strlen(host);
}

static int tmdb(const char *url, int limite, char *saida, size_t tam) {
  static const int ESCADA[] = { 300, 780, 1280 };
  const char *tok = depoisDoHost(url, "image.tmdb.org/t/p/"), *fim;
  int atual, nova;
  char novo[16];
  if (!tok || !(fim = strchr(tok, '/'))) return 0;
  atual = larguraDoToken(tok, (size_t)(fim - tok));
  if (!atual) return 0;
  nova = escolher(ESCADA, 3, atual, limite);
  if (!nova) return 0;
  snprintf(novo, sizeof novo, "w%d", nova);
  return trocar(url, tok, (size_t)(fim - tok), novo, saida, tam);
}

// episodes.metahub.space/<tt>/<temporada>/<episodio>/<tamanho>.jpg — a escada
// que artehero.h descreve: w780 780x439, w1280 1280x720, original o que a
// fonte tiver.
static int episodio(const char *url, int limite, char *saida, size_t tam) {
  static const int ESCADA[] = { 780, 1280 };
  const char *p = depoisDoHost(url, "episodes.metahub.space/"), *tok, *ponto;
  int barras = 0, atual, nova;
  char novo[16];
  if (!p || strchr(p, '?')) return 0;
  for (tok = p; *tok; tok++) if (*tok == '/') barras++;
  if (barras != 3) return 0;
  tok = strrchr(p, '/') + 1;
  ponto = strchr(tok, '.');
  if (!ponto || strncmp(ponto, ".jpg", 4)) return 0;
  atual = larguraDoToken(tok, (size_t)(ponto - tok));
  if (!atual) return 0;
  nova = escolher(ESCADA, 2, atual, limite);
  if (!nova) return 0;
  snprintf(novo, sizeof novo, "w%d", nova);
  return trocar(url, tok, (size_t)(ponto - tok), novo, saida, tam);
}

// images.metahub.space/background/<tamanho>/<tt>/img: small e 480x270 e o
// resto e o mesmo 1920x1080. So o fundo: cartaz e logo do metahub nao tem
// escada medida.
static int fundoMetahub(const char *url, int limite, char *saida, size_t tam) {
  static const char *GRANDES[] = { "medium", "big", "large", "original" };
  const char *tok = depoisDoHost(url, "images.metahub.space/background/"), *fim;
  size_t i;
  if (!tok || limite > 480 || !(fim = strchr(tok, '/'))) return 0;
  for (i = 0; i < sizeof GRANDES / sizeof GRANDES[0]; i++)
    if ((size_t)(fim - tok) == strlen(GRANDES[i]) && !strncmp(tok, GRANDES[i], strlen(GRANDES[i])))
      return trocar(url, tok, (size_t)(fim - tok), "small", saida, tam);
  return 0;
}

int arte_tamanho_url(const char *url, int limite, char *saida, size_t tam) {
  if (!url || !saida || !tam || limite <= 0) return 0;
  return tmdb(url, limite, saida, tam) ||
         episodio(url, limite, saida, tam) ||
         fundoMetahub(url, limite, saida, tam);
}
