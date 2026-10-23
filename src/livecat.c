// Ver livecat.h.
#include "livecat.h"
#include "dados.h"
#include "livetv_regras.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define ARQ "livecat.txt"
#define BASE_MAX 201         // Worker accepts up to 200 UTF-8 bytes
#define LOTE 200             // Worker splits each request into <=64 Jev questions
#define LOTES_MAX 6          // ponytail: cap each load at 1200 misses; remaining names retry next load
#define CACHE_MAX 3000
#define VALIDADE_DIAS 30
#define PRAZO_S 2            // Total network budget: optional suggestions must not delay the guide

static long long agoraMs(void) {
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t)) return -1;
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

// As secoes de categoriaPorNome (chaves de idioma_tab.h), na ordem do Worker.
static const char *const SECAO[] = {
  "Esportes", "Notícias", "Infantil", "Documentários", "Filmes", "Séries",
  "Música", "Variedades", "Abertos",
};
#define NSECAO ((int)(sizeof SECAO / sizeof *SECAO))

typedef struct { char base[BASE_MAX]; signed char sec; int dia; } Ent;   // sec -1 = sem sugestao
static Ent *cache; static int nCache;
static LiveCatPostar postar;

void livecat_postar_teste(LiveCatPostar p) { postar = p; }

static int hoje(void) { return (int)(time(NULL) / 86400); }

static int achar(const char *base) {
  int i;
  for (i = 0; i < nCache; i++) if (!strcmp(cache[i].base, base)) return i;
  return -1;
}

static void guardar(const char *base, int sec) {
  int i = achar(base);
  if (i < 0) {
    if (nCache >= CACHE_MAX) {
      static int proximo;
      i = proximo;
      for (int j = 0; j < nCache; j++)
        if (cache[j].dia < cache[i].dia) i = j;
      proximo = (i + 1) % CACHE_MAX;
    } else i = nCache++;
    snprintf(cache[i].base, sizeof cache[i].base, "%s", base);
  }
  cache[i].sec = (signed char)sec; cache[i].dia = hoje();
}

static void carregar(void) {
  char *txt, *p;
  if (cache) return;
  cache = malloc(sizeof *cache * CACHE_MAX);
  if (!cache) return;
  nCache = 0;
  txt = dados_ler(ARQ);
  for (p = txt; p && *p; ) {
    char *fim = strchr(p, '\n'), *t1, *t2;
    if (fim) *fim = 0;
    t1 = strchr(p, '\t'); t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2) {
      int sec = atoi(t1 + 1), dia = atoi(t2 + 1);
      *t1 = 0;
      if (sec >= -1 && sec < NSECAO && hoje() - dia < VALIDADE_DIAS && nCache < CACHE_MAX && strlen(p) < BASE_MAX) {
        guardar(p, sec); cache[achar(p)].dia = dia;
      }
    }
    p = fim ? fim + 1 : NULL;
  }
  free(txt);
}

// Cached lookup only: safe during startup, with no Worker request or disk write.
const char *livecat_secao(const char *nome, int *resolvido) {
  char base[BASE_MAX];
  int k;
  if (resolvido) *resolvido = 0;
  carregar();
  if (!cache || !nome) return NULL;
  nv_nome_base(nome, base, sizeof base);
  k = achar(base);
  if (k < 0 || cache[k].dia > hoje() ||
      hoje() - cache[k].dia >= VALIDADE_DIAS) return NULL;
  if (resolvido) *resolvido = 1;
  return cache[k].sec < 0 ? NULL : SECAO[cache[k].sec];
}

static void gravar(void) {
  size_t cap = (size_t)nCache * (BASE_MAX + 24) + 1, u = 0;
  char *b = malloc(cap);
  int i;
  if (!b) return;
  for (i = 0; i < nCache; i++)
    u += (size_t)snprintf(b + u, cap - u, "%s\t%d\t%d\n", cache[i].base, cache[i].sec, cache[i].dia);
  b[u] = 0;
  dados_gravar(ARQ, b);
  free(b);
}

// Names can contain quotes or backslashes. Controls and URLs are excluded.
static void escapar(const char *src, char *dst) {
  while (*src) {
    if (*src == '"' || *src == '\\') *dst++ = '\\';
    *dst++ = *src++;
  }
  *dst = 0;
}

// Um lote: POST {"nomes":[...]} e le {"cats":{"<base>":"<secao>"|null}}. Nome
// ausente da resposta (teto do Worker, falha do Jev) nao entra no cache.
static int perguntar(char (*bases)[BASE_MAX], int n, int prazo) {
  static const char *const cab[] = { "Content-Type: application/json", NULL };
  char url[200], *corpo, *resp, *cats;
  size_t u = 0, cap = (size_t)n * (2 * BASE_MAX + 4) + 16;
  int i, st = 0;
  corpo = malloc(cap);
  if (!corpo) return 0;
  u = (size_t)snprintf(corpo, cap, "{\"nomes\":[");
  for (i = 0; i < n; i++) {
    char escaped[BASE_MAX * 2];
    escapar(bases[i], escaped);
    u += (size_t)snprintf(corpo + u, cap - u, "%s\"%s\"", i ? "," : "", escaped);
  }
  snprintf(corpo + u, cap - u, "]}");
  snprintf(url, sizeof url, "%s/livetv/categorias", NV_REC_URL);
  resp = postar ? postar(url, corpo, &st) : rede_postar_st(url, prazo, cab, corpo, &st);
  free(corpo);
  if (!resp || st < 200 || st >= 300) { free(resp); return 0; }
  cats = malloc(strlen(resp) + 1);
  if (!cats || !js_bruto(resp, NULL, "cats", cats, strlen(resp) + 1) || *cats != '{') {
    free(cats); free(resp); return 0;
  }
  for (i = 0; i < n; i++) {
    char chave[BASE_MAX * 2], value[80], section[64];
    int k, sec = -2;
    escapar(bases[i], chave);
    if (!js_bruto(cats, NULL, chave, value, sizeof value)) continue;
    if (!strcmp(value, "null")) sec = -1;
    else if (js_cadeia(value, section, sizeof section))
      for (k = 0; k < NSECAO; k++)
        if (!strcmp(section, SECAO[k])) { sec = k; break; }
    if (sec != -2) guardar(bases[i], sec);
  }
  free(cats); free(resp);
  return 1;
}

int livecat_sugerir(const char *const *nomes, const unsigned char *sem, int n,
                    const char **out, int (*ativo)(void)) {
  char (*falta)[BASE_MAX] = NULL;
  char (*base)[BASE_MAX] = NULL;
  int i, nf = 0, lote, ganhou = 0;
  long long inicio = agoraMs();
  for (i = 0; i < n; i++) out[i] = NULL;
  if (!NV_REC_URL[0] && !postar) return 0;
  base = malloc(sizeof *base * (size_t)(n > 0 ? n : 1));
  falta = malloc(sizeof *falta * LOTE * LOTES_MAX);
  carregar();
  if (!base || !falta || !cache) goto fim;
  // Drop expired entries even when the app stays open for more than 30 days.
  for (i = nCache - 1; i >= 0; i--)
    if (hoje() - cache[i].dia >= VALIDADE_DIAS || cache[i].dia > hoje()) cache[i] = cache[--nCache];
  for (i = 0; i < n; i++) {
    const char *c;
    base[i][0] = 0;
    if (!sem[i] || !nomes[i]) continue;
    if (strstr(nomes[i], "://") || strstr(nomes[i], "//")) continue;
    for (c = nomes[i]; *c; c++)
      if (!strncasecmp(c, "www.", 4) || (unsigned char)*c < 0x20 || *c == 0x7f) break;
    if (*c) continue;
    nv_nome_base(nomes[i], base[i], BASE_MAX);
    if (!base[i][0] || strlen(base[i]) >= BASE_MAX - 1) { base[i][0] = 0; continue; }
    if (achar(base[i]) < 0) {
      int j;
      for (j = 0; j < nf; j++) if (!strcmp(falta[j], base[i])) break;
      if (j == nf && nf < LOTE * LOTES_MAX) snprintf(falta[nf++], BASE_MAX, "%s", base[i]);
    }
  }
  for (lote = 0; lote * LOTE < nf && (!ativo || ativo()); lote++) {
    int m = nf - lote * LOTE < LOTE ? nf - lote * LOTE : LOTE;
    long long agora = agoraMs();
    int prazo = (int)((inicio + PRAZO_S * 1000 - agora) / 1000);
    if (inicio < 0 || agora < inicio || prazo <= 0) break;
    if (!perguntar(falta + lote * LOTE, m, prazo)) break;     // rede fora: nao insiste nesta carga
  }
  if (nf) gravar();
  for (i = 0; i < n; i++) {
    int k = base[i][0] ? achar(base[i]) : -1;
    if (k >= 0 && cache[k].sec >= 0) { out[i] = SECAO[cache[k].sec]; ganhou++; }
  }
fim:
  free(base); free(falta);
  return ganhou;
}
