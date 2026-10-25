// ARM (arm.haglund.dev/api/v2): o MESMO anime em cada site, para a ficha
// (descoberta.c) e para as fontes (addons.c). Publico e sem chave; o unico dado
// enviado e o id. Cobre todas as entradas da API:
//   /ids?source=   anidb, anilist, anime-planet, animecountdown, animenewsnetwork,
//                  anisearch, kitsu, livechart, myanimelist
//   /imdb /themoviedb /thetvdb   o caminho de volta, de um "tt"/"tmdb:"/"tvdb:"
// e devolve o IMDb, a temporada (thetvdb-season / themoviedb-season) e o mesmo
// titulo em cada site ("kitsu:41370" -> tt9335498 T1, mal 38000, anilist 101922).
//
// SO ANIME: o ARM nao conhece outra coisa. Um "tt" que nao e anime volta [] e
// nada muda.
//
// TEMPORADA E EPISODIO. Uma entrada de anime e UMA temporada, ou parte dela. O
// episodio so atravessa entre "kitsu:X:E" e "tt:T:E" quando a temporada T tem
// UMA entrada so no ARM (entao o desfase e zero). Medido em 22/10/2026:
//   Attack on Titan tt2560140: T1, T2 uma entrada cada -> traduz; T3 e T4 tem
//     parte I e parte II (e um especial na T4) -> nao traduz;
//   Mushoku Tensei tt13293588: T1 em duas partes -> nao traduz;
//   One Piece / Naruto: entrada sem temporada -> nao traduz.
// Nunca se pede um episodio errado; no pior caso nao se pede nada a mais.
//
// So funcoes estaticas no cabecalho, como idbase.h: quem precisa nao linka mais
// um modulo (os testes compilam pedacos do app). Cada arquivo que inclui tem o
// seu cache em memoria (respostas por 6 h, falhas por 10 min).
#ifndef NV_ARMIDS_H
#define NV_ARMIDS_H
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "idbase.h"
#include "js.h"
#include "rede.h"

#define ARM_COLS 8
#define ARM_ALT_TAM 96
typedef struct {
  char imdb[32];
  unsigned long id[ARM_COLS];   // colunas de arm_coluna()
  int temporada;                // 0 = o ARM nao diz, ou diz temporadas diferentes
} ArmIds;

// Coluna -> prefixo do id ("kitsu:") e campo da resposta ("kitsu").
static inline const char *arm_prefixo(int col) {
  static const char *const P[ARM_COLS] = { "kitsu:", "mal:", "anilist:", "anidb:",
    "livechart:", "anisearch:", "ann:", "animecountdown:" };
  return col >= 0 && col < ARM_COLS ? P[col] : "";
}
static inline const char *armCampo(int col) {
  static const char *const C[ARM_COLS] = { "kitsu", "myanimelist", "anilist", "anidb",
    "livechart", "anisearch", "animenewsnetwork", "animecountdown" };
  return C[col];
}

enum { ARM_IDS, ARM_IMDB, ARM_TMDB, ARM_TVDB };
// Prefixo de entrada -> fonte da API e rota. `id` avanca o prefixo (o "tt" fica,
// e parte do id). -1 se o ARM nao aceita o id.
static inline int armEntrada(const char **id, const char **fonte) {
  static const struct { const char *pref, *fonte; int via; } E[] = {
    { "kitsu:", "kitsu", ARM_IDS }, { "mal:", "myanimelist", ARM_IDS },
    { "myanimelist:", "myanimelist", ARM_IDS }, { "anilist:", "anilist", ARM_IDS },
    { "anidb:", "anidb", ARM_IDS }, { "livechart:", "livechart", ARM_IDS },
    { "anisearch:", "anisearch", ARM_IDS }, { "ann:", "animenewsnetwork", ARM_IDS },
    { "animenewsnetwork:", "animenewsnetwork", ARM_IDS },
    { "animecountdown:", "animecountdown", ARM_IDS },
    { "animeplanet:", "anime-planet", ARM_IDS }, { "anime-planet:", "anime-planet", ARM_IDS },
    { "tt", "imdb", ARM_IMDB }, { "tmdb:", "themoviedb", ARM_TMDB },
    { "tvdb:", "thetvdb", ARM_TVDB }, { "thetvdb:", "thetvdb", ARM_TVDB },
  };
  size_t i;
  if (!id || !*id) return -1;
  for (i = 0; i < sizeof E / sizeof *E; i++) {
    size_t l = strlen(E[i].pref);
    if (strncmp(*id, E[i].pref, l)) continue;
    if (E[i].via == ARM_IMDB) { if ((*id)[2] < '0' || (*id)[2] > '9') continue; }
    else *id += l;
    if (fonte) *fonte = E[i].fonte;
    return E[i].via;
  }
  return -1;
}

// Id de site de anime (kitsu, mal, anilist...): o ARM sempre vale a pena.
static inline int arm_id_anime(const char *id) {
  return armEntrada(&id, NULL) == ARM_IDS;
}
// O ARM aceita este id como entrada (anime, "tt", "tmdb:", "tvdb:").
static inline int arm_id_conhecido(const char *id) {
  return armEntrada(&id, NULL) >= 0;
}

// --- cache -----------------------------------------------------------------
#define ARM_CACHE_N 32
static struct { char chave[96]; char *corpo; time_t quando; int falha; } armCache[ARM_CACHE_N];
static pthread_mutex_t armCacheTrava = PTHREAD_MUTEX_INITIALIZER;

// Testes: esquece tudo.
static inline void arm_cache_limpar(void) {
  int i;
  pthread_mutex_lock(&armCacheTrava);
  for (i = 0; i < ARM_CACHE_N; i++) { free(armCache[i].corpo); armCache[i].corpo = NULL; armCache[i].chave[0] = 0; }
  pthread_mutex_unlock(&armCacheTrava);
}

// Corpo da resposta (malloc, o chamador libera) ou NULL (falha, HTTP >= 400, ou
// falha recente lembrada). BLOQUEIA ate 8 s na primeira vez.
static inline char *armBaixar(const char *chave, const char *url) {
  int i, vaga = 0, st = 0;
  time_t agora = time(NULL);
  char *c;
  pthread_mutex_lock(&armCacheTrava);
  for (i = 0; i < ARM_CACHE_N; i++) {
    if (!armCache[i].chave[0] || strcmp(armCache[i].chave, chave)) continue;
    if (agora - armCache[i].quando < (armCache[i].falha ? 600 : 6 * 3600)) {
      c = armCache[i].falha || !armCache[i].corpo ? NULL : strdup(armCache[i].corpo);
      pthread_mutex_unlock(&armCacheTrava);
      return c;
    }
    break;
  }
  pthread_mutex_unlock(&armCacheTrava);
  c = rede_baixar_st(url, 8, NULL, &st);
  if (c && st >= 400) { free(c); c = NULL; }
  pthread_mutex_lock(&armCacheTrava);
  for (i = 0; i < ARM_CACHE_N; i++) {
    if (armCache[i].chave[0] && !strcmp(armCache[i].chave, chave)) { vaga = i; break; }
    if (!armCache[i].chave[0] || armCache[i].quando < armCache[vaga].quando) vaga = i;
  }
  free(armCache[vaga].corpo);
  snprintf(armCache[vaga].chave, sizeof armCache[vaga].chave, "%s", chave);
  armCache[vaga].corpo = c ? strdup(c) : NULL;
  armCache[vaga].falha = !c;
  armCache[vaga].quando = agora;
  pthread_mutex_unlock(&armCacheTrava);
  return c;
}

// Corpo do ARM para `id` ("kitsu:41370:5" usa so "kitsu:41370"). `*via` diz a
// rota. NULL se o ARM nao aceita o id ou nao respondeu.
static inline char *armCorpo(const char *id, int *via) {
  static const char *const ROTA[] = { "ids", "imdb", "themoviedb", "thetvdb" };
  const char *fonte = NULL;
  char num[64], chave[96], url[256];
  size_t k = 0;
  int v = armEntrada(&id, &fonte);
  if (v < 0) return NULL;
  if ((v == ARM_TMDB || v == ARM_TVDB) && (id[0] == 't' || id[0] == 'm') && id[1] >= '0' && id[1] <= '9') id++;
  // Numerico; o slug do anime-planet e o IMDb ("tt...") aceitam letras.
  while (k + 1 < sizeof num) {
    char ch = id[k];
    int ok = (ch >= '0' && ch <= '9') ||
             ((v == ARM_IMDB || !strcmp(fonte, "anime-planet")) &&
              ((ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_'));
    if (!ok) break;
    num[k] = ch; k++;
  }
  num[k] = 0;
  if (!k) return NULL;
  snprintf(chave, sizeof chave, "arm/%s:%s", fonte, num);
  if (v == ARM_IDS)
    snprintf(url, sizeof url, "https://arm.haglund.dev/api/v2/ids?source=%s&id=%s", fonte, num);
  else
    snprintf(url, sizeof url, "https://arm.haglund.dev/api/v2/%s?id=%s", ROTA[v], num);
  if (via) *via = v;
  return armBaixar(chave, url);
}

// Temporada de uma entrada: thetvdb-season, ou themoviedb-season; as duas
// presentes e diferentes = 0 (nao se sabe qual a do IMDb).
static inline int armTemporada(const char *p, const char *f) {
  int tv = (int)js_num(p, f, "thetvdb-season", 0.0), md = (int)js_num(p, f, "themoviedb-season", 0.0);
  if (tv > 0 && md > 0 && tv != md) return 0;
  return tv > 0 ? tv : md > 0 ? md : 0;
}

static inline void armLerEntrada(const char *p, const char *f, ArmIds *fora) {
  char tt[32] = "";
  int i;
  if (js_texto_raiz_em(p, f, "imdb", tt, sizeof tt) && !strncmp(tt, "tt", 2))
    snprintf(fora->imdb, sizeof fora->imdb, "%s", tt);
  for (i = 0; i < ARM_COLS; i++) {
    double v = js_num(p, f, armCampo(i), 0.0);
    if (v >= 1.0 && v < 4e9) fora->id[i] = (unsigned long)v;
  }
  fora->temporada = armTemporada(p, f);
}

// Entradas de uma lista ([{...},{...}]) que passam no filtro: `filme` 1 =
// MOVIE, 0 = o resto, -1 = todas; `temporada` > 0 = so as dessa temporada.
// Devolve quantas; a ultima vai para `fora` (zerado antes).
static inline int armDaLista(const char *c, int filme, int temporada, ArmIds *fora) {
  const char *p = c;
  int achadas = 0;
  memset(fora, 0, sizeof *fora);
  while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++;
  if (*p != '[') return 0;
  for (p = strchr(p, '{'); p; p = strchr(p, '{')) {
    const char *f = strchr(p, '}');   // entradas planas, sem { } dentro
    char media[16] = "";
    if (!f) break;
    f++;
    js_texto_raiz_em(p, f, "media", media, sizeof media);
    if ((filme < 0 || (!strcmp(media, "MOVIE")) == filme) &&
        (temporada <= 0 || armTemporada(p, f) == temporada)) {
      achadas++;
      memset(fora, 0, sizeof *fora);
      armLerEntrada(p, f, fora);
    }
    p = f;
  }
  return achadas;
}

// O mesmo titulo em cada site, para a FICHA. `tipo` ("movie"/"series"/outro)
// filtra as listas; uma lista tem de deixar UMA entrada, e de temporada <= 1
// (uma entrada so nao prova uma temporada so: o ARM pode ter mapeado so a 2a).
// "tmdb:"/"tvdb:" so para serie: o "themoviedb" do ARM e o id da SERIE (o filme
// de Cowboy Bebop vem com o tmdb 30991, que e o da serie). 1 se achou; `fora`
// sempre zerado antes.
static inline int arm_consultar(const char *id, const char *tipo, ArmIds *fora) {
  const char *num = id;
  int via = -1, filme = tipo && !strcmp(tipo, "movie") ? 1 : tipo && !strcmp(tipo, "series") ? 0 : -1;
  char *c;
  int ok = 0;
  memset(fora, 0, sizeof *fora);
  if (!id) return 0;
  { int v = armEntrada(&num, NULL);
    if ((v == ARM_TMDB || v == ARM_TVDB) && (num[0] == 't' || num[0] == 'm') && num[1] >= '0' && num[1] <= '9' && filme < 0)
      filme = num[0] == 'm';
    if ((v == ARM_TMDB || v == ARM_TVDB) && filme != 0) return 0; }
  c = armCorpo(id, &via);
  if (!c) return 0;
  { const char *p = c;
    while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++;
    if (*p == '[') ok = armDaLista(c, filme, 0, fora) == 1 && fora->temporada <= 1;
    else if (*p == '{') { armLerEntrada(p, NULL, fora); ok = 1; } }
  if (!ok) memset(fora, 0, sizeof *fora);
  free(c);
  return ok;
}

// Quantas entradas de anime (nao filme) a temporada `t` do IMDb `tt` tem.
static inline int armEntradasNaTemporada(const char *tt, int t) {
  ArmIds lixo;
  char *c = armCorpo(tt, NULL);
  int n = c ? armDaLista(c, 0, t, &lixo) : 0;
  free(c);
  return n;
}

// IDS PARA AS FONTES: o mesmo filme/episodio de `id` em outros espacos de ids,
// ja no formato do /stream ("tt9335498:1:5", "kitsu:41370:5", "mal:38000:5",
// "kitsu:44388" para filme). `tipo` "movie"/"series". Ate `max` em `alt`;
// devolve quantos. 0 para o que nao e anime, canal, ou episodio sem traducao
// segura (ver TEMPORADA E EPISODIO acima).
static inline int arm_ids_stream(const char *id, const char *tipo, char (*alt)[ARM_ALT_TAM], int max) {
  char base[64];
  int t = 0, e = 0, n = 0, k, filme;
  ArmIds a;
  if (!id || !tipo || max <= 0) return 0;
  if (!strcmp(tipo, "movie")) filme = 1;
  else if (!strcmp(tipo, "series")) filme = 0;
  else return 0;
  idbase_copiar(id, base, sizeof base);
  idbase_episodio(id, &t, &e);
  if (!filme && e <= 0) return 0;
  if (arm_id_anime(base)) {
    // "kitsu:X:E": o episodio e o da entrada. "kitsu:X:T:E" nao e convencao de
    // ninguem; nao se traduz.
    if (!filme && t >= 0) return 0;
    if (!arm_consultar(base, tipo, &a)) return 0;
    for (k = 0; k < ARM_COLS && n < max; k++) {
      char s[ARM_ALT_TAM];
      if (!a.id[k]) continue;
      if (filme) snprintf(s, sizeof s, "%s%lu", arm_prefixo(k), a.id[k]);
      else snprintf(s, sizeof s, "%s%lu:%d", arm_prefixo(k), a.id[k], e);
      if (!strncmp(s, base, strlen(base)) && (s[strlen(base)] == 0 || s[strlen(base)] == ':')) continue;
      snprintf(alt[n++], ARM_ALT_TAM, "%s", s);
    }
    if (a.imdb[0] && n < max) {
      if (filme) snprintf(alt[n++], ARM_ALT_TAM, "%s", a.imdb);
      else if (a.temporada > 0 && armEntradasNaTemporada(a.imdb, a.temporada) == 1)
        snprintf(alt[n++], ARM_ALT_TAM, "%s:%d:%d", a.imdb, a.temporada, e);
    }
    return n;
  }
  if (idbase_e_imdb(base)) {
    char *c = armCorpo(base, NULL);
    int achadas;
    if (!c) return 0;
    if (!filme && t <= 0) { free(c); return 0; }   // especiais (T0): sem traducao
    achadas = armDaLista(c, filme, filme ? 0 : t, &a);
    free(c);
    if (achadas != 1) return 0;
    for (k = 0; k < ARM_COLS && n < max; k++) {
      if (!a.id[k]) continue;
      if (filme) snprintf(alt[n++], ARM_ALT_TAM, "%s%lu", arm_prefixo(k), a.id[k]);
      else snprintf(alt[n++], ARM_ALT_TAM, "%s%lu:%d", arm_prefixo(k), a.id[k], e);
    }
    return n;
  }
  return 0;
}

#endif
