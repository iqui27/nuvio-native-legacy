// metaep_tvdb_indice/metaep_tvdb_global must answer exactly as the per-episode
// scan they replace (animeTvdbGlobal in src/simkl.c before the table): same
// "yes/no" and the same episode for every season and episode asked.
#include "../src/metaep.h"
#include "../src/js.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The old lookup, verbatim: one full scan of the body per question.
static int animeTvdbGlobal(const char *corpo, int temporada, int episodio,
                           MetaEpInfo *mapeado) {
  const char *p;
  unsigned char encontrados[SHRT_MAX + 1] = {0};
  int primeiro = INT_MAX, ultimo = 0, anteriorMax = 0;
  if (!corpo || temporada <= 1 || episodio <= 0 ||
      episodio > SHRT_MAX || js_videos_temporadas_falsas(corpo)) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *fim = js_fim(p);
    int t = metaep_temporada(p, fim);
    int e = (int)js_num(p, fim, "episode", 0);
    if (t == temporada - 1 && e > anteriorMax) anteriorMax = e;
    if (t != temporada || e <= 0) continue;
    if (e < primeiro) primeiro = e;
    if (e > ultimo) ultimo = e;
  }
  if (primeiro <= 1 || anteriorMax != primeiro - 1 ||
      episodio > ultimo - primeiro + 1) return 0;
  int alvo = primeiro + episodio - 1;
  int quantidade = 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *fim = js_fim(p);
    int e = (int)js_num(p, fim, "episode", 0);
    if (metaep_temporada(p, fim) != temporada || e < primeiro || e > alvo) continue;
    if (encontrados[e - primeiro]) return 0;
    encontrados[e - primeiro] = 1;
    quantidade++;
  }
  if (quantidade != episodio) return 0;
  if (mapeado) {
    memset(mapeado, 0, sizeof *mapeado);
    mapeado->temporada = temporada;
    mapeado->episodio = alvo;
  }
  return 1;
}

static char buf[1 << 21];
static size_t pos;

static void ini(void) { pos = (size_t)snprintf(buf, sizeof buf, "{\"videos\":["); }
static void ep(int t, int e) {
  pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%s{\"season\":%d,\"episode\":%d}",
                          buf[pos - 1] == '[' ? "" : ",", t, e);
}
static void ep_sem_temporada(int e) {
  pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%s{\"episode\":%d}",
                          buf[pos - 1] == '[' ? "" : ",", e);
}
static void fim(void) { snprintf(buf + pos, sizeof buf - pos, "]}"); }

static int casos;

static void comparar(const char *nome) {
  MetaTvdbIndice ix;
  int t, e, sim = 0;
  assert(metaep_tvdb_indice(buf, &ix));
  // Past every season (22) and every season's length (118), with room.
  for (t = -1; t <= 25; t++)
    for (e = -1; e <= 140; e++) {
      MetaEpInfo a, b;
      int va, vb;
      memset(&a, 0x55, sizeof a);
      memset(&b, 0x55, sizeof b);
      va = animeTvdbGlobal(buf, t, e, &a);
      vb = metaep_tvdb_global(&ix, t, e, &b);
      if (va != vb || (va && (a.temporada != b.temporada || a.episodio != b.episodio))) {
        fprintf(stderr, "FALHA %s: S%dE%d antigo=%d S%dE%d novo=%d S%dE%d\n", nome, t, e,
                va, va ? a.temporada : 0, va ? a.episodio : 0,
                vb, vb ? b.temporada : 0, vb ? b.episodio : 0);
        exit(1);
      }
      sim += va;
      casos++;
    }
  metaep_tvdb_indice_liberar(&ix);
  printf("ok  %-44s %5d episodios achados\n", nome, sim);
}

int main(void) {
  // One Piece as Nuvio numbers it: absolute numbers across seasons, out of
  // order in the body, with specials (season 0) mixed in.
  // 22 seasons, 1..1122 (sizes add up to 1122).
  static const int tam[] = {61, 16, 14, 39, 13, 52, 33, 35, 62, 49, 118, 33, 98,
                            14, 100, 57, 102, 115, 74, 9, 9, 19};
  int t, e, n;
  ini();
  for (t = 22, n = 1122; t >= 1; t--) {
    int primeiro = n - tam[t - 1] + 1;
    for (e = primeiro; e <= n; e++) ep(t, e);
    n = primeiro - 1;
  }
  for (e = 1; e <= 12; e++) ep(0, e);
  assert(n == 0);
  fim();
  comparar("one piece absoluto, fora de ordem");

  // Per-season numbering (Xperience/TVDB): every season starts at 1, so the
  // absolute rule never applies.
  ini();
  for (t = 1; t <= 16; t++) for (e = 1; e <= 20; e++) ep(t, e);
  fim();
  comparar("numeracao por temporada");

  // A gap inside a season: answers stop at the missing number.
  ini();
  for (e = 1; e <= 20; e++) ep(1, e);
  for (e = 21; e <= 40; e++) if (e != 30) ep(2, e);
  for (e = 41; e <= 50; e++) ep(3, e);
  fim();
  comparar("buraco no meio da temporada");

  // A repeated number: answers stop before it, even past it in order.
  ini();
  for (e = 1; e <= 20; e++) ep(1, e);
  for (e = 21; e <= 40; e++) { ep(2, e); if (e == 25) ep(2, 25); }
  for (e = 41; e <= 50; e++) ep(3, e);
  fim();
  comparar("numero repetido");

  // The first episode repeated, and a season that does not continue the last.
  ini();
  for (e = 1; e <= 20; e++) ep(1, e);
  ep(2, 21); ep(2, 21); for (e = 22; e <= 30; e++) ep(2, e);
  for (e = 35; e <= 40; e++) ep(3, e);
  fim();
  comparar("primeiro repetido, temporada sem emenda");

  // Missing previous season, zero/negative numbers, a video with no season
  // (counts as season 1), and an empty season number.
  ini();
  for (e = 1; e <= 10; e++) ep_sem_temporada(e);
  ep(1, 0); ep(1, -3);
  for (e = 11; e <= 20; e++) ep(2, e);
  ep(2, 0);
  for (e = 31; e <= 40; e++) ep(4, e);
  fim();
  comparar("sem temporada, zeros, temporada faltando");

  // Five fake seasons (every one E1 dated Jan 1): the body is not trusted.
  ini();
  pos += (size_t)snprintf(buf + pos, sizeof buf - pos,
    "{\"season\":1,\"episode\":1,\"released\":\"2001-01-01\"},"
    "{\"season\":2,\"episode\":1,\"released\":\"2002-01-01\"},"
    "{\"season\":3,\"episode\":1,\"released\":\"2003-01-01\"},"
    "{\"season\":4,\"episode\":1,\"released\":\"2004-01-01\"},"
    "{\"season\":5,\"episode\":1,\"released\":\"2005-01-01\"}");
  fim();
  comparar("temporadas falsas");

  // No videos at all, and no body.
  snprintf(buf, sizeof buf, "{\"name\":\"x\"}");
  comparar("sem videos");
  { MetaTvdbIndice ix; MetaEpInfo m;
    assert(!metaep_tvdb_indice(NULL, &ix));
    assert(!metaep_tvdb_global(&ix, 2, 1, &m));
    metaep_tvdb_indice_liberar(&ix); }

  printf("ok  %d perguntas, todas iguais a busca antiga\n", casos);
  return 0;
}
