// Ver pecafio.h.
#include "pecafio.h"
#include "rede.h"
#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  unsigned id;
  char chave[96];
  char *corpo;
  char *nomes;             // os titulos do contexto, copiados (um bloco)
  PecaContexto ctx;
} Pedido;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned corrente, serie;
static int ocupado, pronto;
static PecaResultado resultado;
static PecaSugestao achados[PECA_N];
static int nAchados, msG, msC;

// So no preview de desenvolvimento do Linux: tests/peca-e2e.sh aponta o fio
// para um Gemini e um Cinemeta de mentira. Em TV estes nomes nao existem.
static const char *urlGemini(void) {
#ifdef NV_LINUX_DESKTOP
  const char *e = getenv("NV_PECA_GEMINI_URL");
  if (e && *e) return e;
#endif
  return PECA_URL;
}
static const char *baseCinemeta(void) {
#ifdef NV_LINUX_DESKTOP
  const char *e = getenv("NV_PECA_CINEMETA_URL");
  if (e && *e) return e;
#endif
  return NULL;
}

static int abandonado(unsigned id) {
  int r;
  pthread_mutex_lock(&mtx); r = id != corrente; pthread_mutex_unlock(&mtx);
  return r;
}

static void *buscar(void *arg) {
  Pedido *pd = arg;
  PecaSugestao cand[PECA_CANDIDATOS], ok[PECA_N];
  char cab[128];
  const char *cabs[2] = { cab, NULL }, *url = urlGemini();
  int http = 0, n = 0, nOk = 0, tg, tc = 0;
  Uint32 t0 = SDL_GetTicks();
  PecaResultado res;
  char *r;
  snprintf(cab, sizeof cab, "x-goog-api-key: %s", pd->chave);
  r = strncmp(url, "https://", 8) ? rede_postar_st(url, 45, cabs, pd->corpo, &http)
                                  : rede_postar_seguro_st(url, 45, cabs, pd->corpo, &http);
  memset(cab, 0, sizeof cab);
  memset(pd->chave, 0, sizeof pd->chave);
  res = peca_resposta(http, r, cand, PECA_CANDIDATOS, &n);
  free(r);
  tg = (int)(SDL_GetTicks() - t0);
  if (res == PECA_OK) {
    Uint32 t1 = SDL_GetTicks();
    // FONTE VERIFICADA: so entra o que o Cinemeta confirma (nome e ano). O
    // modelo pode inventar; a TV nao mostra titulo que nao sabe abrir.
    for (int i = 0; i < n && nOk < PECA_N && !abandonado(pd->id); i++) {
      int dup = 0;
      if (peca_ja_visto(&pd->ctx, &cand[i])) continue;
      for (int orig = 1; orig >= 0 && !cand[i].id[0]; orig--) {
        char u[700], *m;
        if (!orig && !strcmp(cand[i].titulo, cand[i].original)) break;
        if (!peca_cinemeta_url(baseCinemeta(), &cand[i], orig, u, sizeof u)) continue;
        m = rede_baixar(u, 12);
        peca_cinemeta_escolher(m, &cand[i]);
        free(m);
      }
      if (!cand[i].id[0]) continue;
      for (int k = 0; k < nOk; k++) if (!strcmp(ok[k].id, cand[i].id)) dup = 1;
      if (!dup) ok[nOk++] = cand[i];
    }
    tc = (int)(SDL_GetTicks() - t1);
    if (!nOk) res = PECA_VAZIO;
  }
  printf("[peca] resultado %d: http %d, %d sugeridos, %d confirmados, gemini %d ms, cinemeta %d ms\n",
         (int)res, http, n, nOk, tg, tc);
  pthread_mutex_lock(&mtx);
  if (pd->id == corrente) {
    resultado = res; nAchados = nOk; msG = tg; msC = tc;
    memcpy(achados, ok, sizeof(PecaSugestao) * (size_t)nOk);
    ocupado = 0; pronto = 1;
  }
  pthread_mutex_unlock(&mtx);
  free(pd->corpo); free(pd->nomes); free(pd);
  return NULL;
}

// Copia os nomes de um grupo para o bloco e reaponta.
static void copiar(const char *const *de, int n, const char **para, int *np, char **w) {
  *np = 0;
  for (int i = 0; i < n && i < PECA_CTX_MAX; i++) {
    if (!de[i] || !de[i][0]) continue;
    snprintf(*w, 161, "%.160s", de[i]);
    para[(*np)++] = *w;
    *w += strlen(*w) + 1;
  }
}

int pecafio_pedir(const char *chave, const char *texto, const char *idioma, const PecaContexto *ctx) {
  Pedido *pd;
  pthread_t f;
  if (!chave || !*chave || !texto || !*texto || !(pd = calloc(1, sizeof *pd))) return 0;
  snprintf(pd->chave, sizeof pd->chave, "%s", chave);
  pd->corpo = peca_pedido(texto, idioma, ctx);
  pd->nomes = calloc(3 * PECA_CTX_MAX, 161);
  if (pd->corpo && pd->nomes && ctx) {
    char *w = pd->nomes;
    copiar(ctx->vistos, ctx->nVistos, pd->ctx.vistos, &pd->ctx.nVistos, &w);
    copiar(ctx->andamento, ctx->nAndamento, pd->ctx.andamento, &pd->ctx.nAndamento, &w);
    copiar(ctx->lista, ctx->nLista, pd->ctx.lista, &pd->ctx.nLista, &w);
  }
  if (!pd->corpo || !pd->nomes) goto falha;
  pthread_mutex_lock(&mtx);
  if (!++serie) serie = 1;
  pd->id = corrente = serie;
  ocupado = 1; pronto = 0;
  pthread_mutex_unlock(&mtx);
  if (pthread_create(&f, NULL, buscar, pd) == 0) { pthread_detach(f); return 1; }
  pecafio_cancelar();
falha:
  memset(pd->chave, 0, sizeof pd->chave);
  free(pd->corpo); free(pd->nomes); free(pd);
  return 0;
}

int pecafio_ocupado(void) {
  int r;
  pthread_mutex_lock(&mtx); r = ocupado; pthread_mutex_unlock(&mtx);
  return r;
}

int pecafio_pegar(PecaResultado *r, PecaSugestao *v, int *n, int *msGemini, int *msCinemeta) {
  int tem;
  pthread_mutex_lock(&mtx);
  if ((tem = pronto)) {
    pronto = 0;
    if (r) *r = resultado;
    if (n) *n = nAchados;
    if (v) memcpy(v, achados, sizeof(PecaSugestao) * (size_t)nAchados);
    if (msGemini) *msGemini = msG;
    if (msCinemeta) *msCinemeta = msC;
  }
  pthread_mutex_unlock(&mtx);
  return tem;
}

void pecafio_cancelar(void) {
  pthread_mutex_lock(&mtx);
  corrente = 0; ocupado = 0; pronto = 0;
  pthread_mutex_unlock(&mtx);
}
