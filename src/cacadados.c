// A parte PURA da Caca: classificar um credito, escopo, datas e os leitores de
// JSON (TMDB movie_credits e o formato do Worker). Sem rede, sem disco, sem
// estado: so caca.h + js.c, para o teste no Mac (tests/caca.c) ligar so isto.
#include "caca.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMDB_IMG "https://image.tmdb.org/t/p/w185"

int caca_classe(const int *generos, int nGeneros, int duracao) {
  int i;
  for (i = 0; generos && i < nGeneros; i++) if (generos[i] == 10770) return CACA_TV;
  for (i = 0; generos && i < nGeneros; i++) if (generos[i] == 99) return CACA_DOC;
  // < 40 min e curta (a regra da Academia). 0 = nao se sabe ainda: conta como
  // longa ate a duracao chegar, e e por isso que "Completa" espera a fila.
  if (duracao > 0 && duracao < 40) return CACA_CURTA;
  return CACA_LONGA;
}

int caca_no_escopo(int classe, int escopo) {
  if (classe == CACA_TV) return 0;
  if (classe == CACA_DOC) return (escopo & CACA_ESC_DOC) != 0;
  if (classe == CACA_CURTA) return (escopo & CACA_ESC_CURTA) != 0;
  return 1;
}

long caca_data(const char *iso) {
  int a = 0, m = 0, d = 0;
  if (!iso || strlen(iso) < 4) return 0;
  if (sscanf(iso, "%4d-%2d-%2d", &a, &m, &d) < 1 || a < 1870 || a > 2200) return 0;
  if (m < 1 || m > 12) m = 1;
  if (d < 1 || d > 31) d = 1;
  return (long)a * 10000L + m * 100L + d;
}

// strstr limitado a [p,fim): o corpo inteiro de movie_credits passa de 100 KB
// e um strstr por elemento varreria o documento todo de novo.
static const char *achar(const char *p, const char *fim, const char *k) {
  size_t kn = strlen(k);
  for (; p && p + kn <= fim; p++) if (*p == *k && !memcmp(p, k, kn)) return p;
  return NULL;
}

// "genre_ids":[18,99] dentro de [p,fim). Devolve quantos (ate max).
static int generos(const char *p, const char *fim, int *dst, int max) {
  const char *g = p, *k = "\"genre_ids\"";
  size_t kn = strlen(k);
  int n = 0;
  while (g && g < fim) {
    g = achar(g, fim, k);
    if (!g) return 0;
    g += kn;
    while (g < fim && (*g == ' ' || *g == ':')) g++;
    if (g < fim && *g == '[') break;
  }
  if (!g || g >= fim) return 0;
  g++;
  while (g < fim && *g != ']' && n < max) {
    char *e;
    long v = strtol(g, &e, 10);
    if (e == g) { g++; continue; }
    dst[n++] = (int)v;
    g = e;
  }
  return n;
}

static void poster(const char *caminho, char *dst, size_t tam) {
  if (caminho && caminho[0] == '/') snprintf(dst, tam, "%s%s", TMDB_IMG, caminho);
  else if (caminho && !strncmp(caminho, "http", 4)) snprintf(dst, tam, "%s", caminho);
  else if (tam) dst[0] = 0;
}

// Participacao "como ele mesmo" (documentario sobre outra pessoa, making of):
// nao e filmografia de atuacao.
static int comoSiMesmo(const char *papel) {
  static const char *const M[] = { "Self", "Himself", "Herself", "Themselves",
                                   "uncredited", "Narrator (voice)", NULL };
  int i;
  for (i = 0; M[i]; i++) if (strstr(papel, M[i])) return 1;
  return 0;
}

static int jaTem(const CacaFilme *v, int n, long tmdb) {
  int i;
  for (i = 0; i < n; i++) if (v[i].tmdb == tmdb) return 1;
  return 0;
}

static void ordenarPorData(CacaFilme *v, int n) {
  int i, j;
  for (i = 1; i < n; i++) {
    CacaFilme t = v[i];
    for (j = i; j > 0 && v[j - 1].data > t.data; j--) v[j] = v[j - 1];
    v[j] = t;
  }
}

int caca_ler_creditos(const char *json, int papel, long hoje, CacaFilme *dst, int max) {
  const char *p;
  int n = 0;
  if (!json || !dst || max <= 0) return 0;
  p = js_array(json, NULL, papel == CACA_DIRECAO ? "crew" : "cast");
  while (p && n < max) {
    const char *fim = js_fim(p);
    char job[48] = "", papelTxt[160] = "", data[16] = "", cam[128] = "";
    int g[16], ng, adulto;
    long id;
    CacaFilme f;
    if (!fim) break;
    memset(&f, 0, sizeof f);
    id = (long)js_num(p, fim, "id", 0.0);
    js_texto(p, fim, "job", job, sizeof job);
    js_texto(p, fim, "character", papelTxt, sizeof papelTxt);
    js_texto(p, fim, "release_date", data, sizeof data);
    js_texto(p, fim, "poster_path", cam, sizeof cam);
    adulto = achar(p, fim, "\"adult\":true") != NULL;
    ng = generos(p, fim, g, 16);
    f.tmdb = id;
    f.data = caca_data(data);
    f.classe = caca_classe(g, ng, 0);
    js_texto(p, fim, "title", f.titulo, sizeof f.titulo);
    poster(cam, f.poster, sizeof f.poster);
    if (id > 0 && f.titulo[0] && !adulto && f.data > 0 && (hoje <= 0 || f.data <= hoje) &&
        (papel != CACA_DIRECAO || !strcmp(job, "Director")) &&
        (papel != CACA_ATUACAO || !comoSiMesmo(papelTxt)) && !jaTem(dst, n, id))
      dst[n++] = f;
    p = js_prox(fim);
  }
  ordenarPorData(dst, n);
  return n;
}

int caca_ler_filmes(const char *json, CacaFilme *dst, int max) {
  const char *p;
  int n = 0;
  if (!json || !dst || max <= 0) return 0;
  p = js_array(json, NULL, "filmes");
  while (p && n < max) {
    const char *fim = js_fim(p);
    char cam[160] = "", data[16] = "";
    CacaFilme f;
    double d;
    if (!fim) break;
    memset(&f, 0, sizeof f);
    f.tmdb = (long)js_num(p, fim, "tmdb", 0.0);
    js_texto(p, fim, "imdb", f.imdb, sizeof f.imdb);
    js_texto(p, fim, "titulo", f.titulo, sizeof f.titulo);
    js_texto(p, fim, "poster", cam, sizeof cam);
    poster(cam, f.poster, sizeof f.poster);
    // `data` numerica (AAAAMMDD) ou texto ISO: o Worker manda numero, um JSON
    // escrito a mao pode mandar a data.
    if (js_texto(p, fim, "data", data, sizeof data)) f.data = caca_data(data);
    else if ((d = js_num(p, fim, "data", -1.0)) > 0) f.data = (long)d;
    f.classe = (int)js_num(p, fim, "classe", 0.0);
    if (f.classe < CACA_LONGA || f.classe > CACA_TV) f.classe = CACA_LONGA;
    if (strncmp(f.imdb, "tt", 2)) f.imdb[0] = 0;
    if ((f.tmdb > 0 || f.imdb[0]) && !(f.tmdb > 0 && jaTem(dst, n, f.tmdb))) dst[n++] = f;
    p = js_prox(fim);
  }
  return n;
}
