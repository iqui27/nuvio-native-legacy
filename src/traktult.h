#ifndef NV_TRAKTULT_H
#define NV_TRAKTULT_H
// O "A SEGUIR" DO TRAKT SEM SUGERIR O QUE JA FOI VISTO (issue #213).
//
// Relato (LG G5, log 16676): fonte Trakt, 6 cards, 3 de series ja vistas que
// o Trakt nao poe em "continuar". Marcar como visto no app nao tirava. MEDIDO
// no log: seis series com o MESMO watched_at (1790885880000, marcadas de uma
// vez) davam "ultimo visto T2E1", "T1E1", "T1E2"; depois de marcar a serie
// inteira no app (POST /sync/history, HTTP 201; "50 episodios no mapa (50
// vistos)") o historico voltou com o instante novo e AINDA "ultimo visto T2E1"
// e "T1E1". Quem marca a serie inteira grava todos os episodios no mesmo
// instante, e entre empatados o historico nao vem em ordem de episodio: "a
// primeira ocorrencia e a ultima" pegava um episodio do meio.
//
// Duas guardas, aqui sem rede para o teste (tests/traktult.c):
//  1. tk_ult_anotar: no empate de instante fica o MAIOR (temporada, episodio).
//  2. tk_prog_proximo: le o next_episode de /shows/<id>/progress/watched, que
//     e o "a seguir" do proprio Trakt. null = a serie acabou para o Trakt e o
//     card nao entra; com episodio, e ele que entra.
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { char imdb[24]; int temporada, episodio; long long quandoMs; int tvdbEp; } TkUltimo;

// Uma linha de episodio do historico (que vem do mais novo para o mais velho).
// Devolve 1 se criou ou trocou a entrada da serie.
static inline int tk_ult_anotar(TkUltimo *v, int *n, int max, const char *imdb,
                                int t, int e, long long ms) {
  int k;
  if (!imdb || !imdb[0]) return 0;
  for (k = 0; k < *n; k++) {
    if (strcmp(v[k].imdb, imdb)) continue;
    // Mais velho que o anotado: e passado. Mais novo nao deveria vir depois
    // (ordem do historico), mas se vier, ele vence. Empate: o maior episodio.
    if (ms < v[k].quandoMs) return 0;
    if (ms == v[k].quandoMs &&
        (t < v[k].temporada || (t == v[k].temporada && e <= v[k].episodio))) return 0;
    v[k].temporada = t; v[k].episodio = e; v[k].quandoMs = ms; v[k].tvdbEp = 0;
    return 1;
  }
  if (*n >= max) return 0;
  snprintf(v[*n].imdb, sizeof v[*n].imdb, "%s", imdb);
  v[*n].temporada = t; v[*n].episodio = e; v[*n].quandoMs = ms; v[*n].tvdbEp = 0;
  (*n)++;
  return 1;
}

static inline int tk_ult_anotar_tvdb(TkUltimo *v, int *n, int max, const char *id,
                                    int t, int e, long long ms, int tvdbEp) {
  if (!tk_ult_anotar(v, n, max, id, t, e, ms)) return 0;
  for (int k = 0; k < *n; k++) if (!strcmp(v[k].imdb, id)) { v[k].tvdbEp = tvdbEp; break; }
  return 1;
}

// Corpo de /shows/<id>/progress/watched. 1 = proximo em *t/*e; 0 = next_episode
// null (nada a seguir para o Trakt); -1 = nao deu para ler (fica o palpite do
// historico, como antes).
static inline int tk_prog_proximo(const char *corpo, int *t, int *e) {
  const char *p, *s, *nn, *fim;
  int depth;
  if (!corpo) return -1;
  p = strstr(corpo, "\"next_episode\"");
  if (!p) return -1;
  p += 14;
  while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (!strncmp(p, "null", 4)) return 0;
  if (*p != '{') return -1;
  // So o nivel de cima do objeto: "ids" aninhado tem "tvdb"/"trakt", nao
  // "season"/"number", mas o limite evita ler o objeto seguinte.
  for (fim = p, depth = 0; *fim; fim++) {
    if (*fim == '{') depth++;
    else if (*fim == '}' && --depth == 0) break;
  }
  s = strstr(p, "\"season\"");
  nn = strstr(p, "\"number\"");
  if (!s || !nn || s > fim || nn > fim) return -1;
  s += 8; nn += 8;
  while (*s == ' ' || *s == ':') s++;
  while (*nn == ' ' || *nn == ':') nn++;
  *t = atoi(s); *e = atoi(nn);
  return *e > 0 ? 1 : -1;
}

#endif
