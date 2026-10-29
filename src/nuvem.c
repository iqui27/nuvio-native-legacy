#include "nuvem.h"
#include "rede.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_SUPABASE_URL
#define NV_SUPABASE_URL ""
#endif
#ifndef NV_SUPABASE_ANON_KEY
#define NV_SUPABASE_ANON_KEY ""
#endif
#ifndef NV_TV_LOGIN_BASE
#define NV_TV_LOGIN_BASE ""
#endif
#ifndef NV_TRAKT_CLIENT_ID
#define NV_TRAKT_CLIENT_ID ""
#endif
#ifndef NV_TRAKT_CLIENT_SECRET
#define NV_TRAKT_CLIENT_SECRET ""
#endif
#ifndef NV_SIMKL_CLIENT_ID
#define NV_SIMKL_CLIENT_ID ""
#endif
#ifndef NV_SIMKL_APP
#define NV_SIMKL_APP "nuvio"
#endif

static char url[300]      = NV_SUPABASE_URL;
static char anon[1200]    = NV_SUPABASE_ANON_KEY;
static char baseLogin[300] = NV_TV_LOGIN_BASE;
static char traktCliente[128] = NV_TRAKT_CLIENT_ID;
static char traktSegredo[128] = NV_TRAKT_CLIENT_SECRET;
static char simklCliente[200] = NV_SIMKL_CLIENT_ID;
static char simklApp[80] = NV_SIMKL_APP;

// Freio: instante (em segundos) ate quando ninguem tenta, e quantas falhas
// seguidas ja aconteceram. O intervalo dobra a cada falha ate o teto — o mesmo
// desenho do syncBackoffPolicy do web.
#define FREIO_MIN   5
#define FREIO_MAX 300
static long freioAte = 0;
static int  falhas = 0;

static void tiraFim(char *s) {
  char *f = s + strlen(s);
  while (f > s && (f[-1] == '\n' || f[-1] == '\r' || f[-1] == ' ' || f[-1] == '\t')) *--f = 0;
}

int nuvem_configurar(const char *dirArte) {
  char caminho[600], linha[1200];
  FILE *f;
  if (dirArte && *dirArte) {
    snprintf(caminho, sizeof caminho, "%s/nuvem.txt", dirArte);
    f = fopen(caminho, "r");
    if (f) {
      if (fgets(linha, sizeof linha, f)) { tiraFim(linha); if (linha[0]) snprintf(url, sizeof url, "%s", linha); }
      if (fgets(linha, sizeof linha, f)) { tiraFim(linha); if (linha[0]) snprintf(anon, sizeof anon, "%s", linha); }
      if (fgets(linha, sizeof linha, f)) { tiraFim(linha); if (linha[0]) snprintf(baseLogin, sizeof baseLogin, "%s", linha); }
      fclose(f);
      printf("[nuvem] configuracao de art/nuvem.txt\n");
    }
  }
  // Barra no fim quebraria toda concatenacao com o caminho da RPC: o web
  // normaliza a base do mesmo jeito.
  { char *fim = url + strlen(url);
    while (fim > url && fim[-1] == '/') *--fim = 0; }
  { char *fim = baseLogin + strlen(baseLogin);
    while (fim > baseLogin && fim[-1] == '/') *--fim = 0; }

  if (!url[0] || !anon[0]) {
    // Dizer isto alto importa: sem configuracao o app nao "fica sem sync", ele
    // fica sem LOGIN, e a tela vai parecer quebrada sem explicar por que.
    printf("[nuvem] SEM CONFIGURACAO (url=%s chave=%s): login e sync desligados\n",
           url[0] ? "ok" : "vazia", anon[0] ? "ok" : "vazia");
    return 0;
  }
  printf("[nuvem] %s\n", url);
  return 1;
}

const char *nuvem_ultimo_erro(void) { return rede_ultimo_erro(); }
int         nuvem_pronta(void)     { return url[0] && anon[0]; }
const char *nuvem_url(void)        { return url; }
const char *nuvem_anon(void)       { return anon; }
const char *nuvem_base_login(void) { return baseLogin; }
const char *nuvem_trakt_cliente(void) { return traktCliente; }
const char *nuvem_trakt_segredo(void) { return traktSegredo; }
const char *nuvem_simkl_cliente(void) { return simklCliente; }
const char *nuvem_simkl_app(void) { return simklApp; }

char *nuvem_post(const char *caminho, const char *corpoJson,
                 const char *bearer, int *status) {
  char completo[900];
  char cabApi[1300], cabAut[1300];
  const char *cab[3];
  if (status) *status = 0;
  if (!nuvem_pronta() || !caminho) return NULL;

  snprintf(completo, sizeof completo, "%s%s%s", url,
           caminho[0] == '/' ? "" : "/", caminho);
  snprintf(cabApi, sizeof cabApi, "apikey: %s", anon);
  snprintf(cabAut, sizeof cabAut, "Authorization: Bearer %s",
           (bearer && *bearer) ? bearer : anon);
  cab[0] = cabApi;
  cab[1] = cabAut;
  cab[2] = NULL;
  // Content-Type: application/json ja e posto por rede_postar.
  return rede_postar_st(completo, 25, cab, corpoJson ? corpoJson : "{}", status);
}

char *nuvem_rpc_com(const char *funcao, const char *corpoJson,
                    const char *bearer, int *status) {
  char caminho[300];
  if (!funcao || !*funcao) return NULL;
  snprintf(caminho, sizeof caminho, "/rest/v1/rpc/%s", funcao);
  return nuvem_post(caminho, corpoJson, bearer, status);
}

void nuvem_url_escapar(const char *valor, char *dst, unsigned tam) {
  static const char *hex = "0123456789ABCDEF";
  unsigned w = 0;
  const unsigned char *p;
  if (!dst || tam == 0) return;
  dst[0] = 0;
  for (p = (const unsigned char *)(valor ? valor : ""); *p; p++) {
    int seguro = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                 (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
                 *p == '.' || *p == '~';
    if (seguro) {
      if (w + 2 > tam) break;
      dst[w++] = (char)*p;
    } else {
      if (w + 4 > tam) break;
      dst[w++] = '%';
      dst[w++] = hex[*p >> 4];
      dst[w++] = hex[*p & 15];
    }
  }
  dst[w] = 0;
}

char *nuvem_tabela(const char *tabela, const char *consulta,
                   const char *bearer, int *status) {
  char completo[1200], cabApi[1300], cabAut[1300];
  const char *cab[3];
  if (status) *status = 0;
  if (!nuvem_pronta() || !tabela) return NULL;
  snprintf(completo, sizeof completo, "%s/rest/v1/%s%s%s", url, tabela,
           (consulta && *consulta) ? "?" : "", (consulta && *consulta) ? consulta : "");
  snprintf(cabApi, sizeof cabApi, "apikey: %s", anon);
  snprintf(cabAut, sizeof cabAut, "Authorization: Bearer %s",
           (bearer && *bearer) ? bearer : anon);
  cab[0] = cabApi;
  cab[1] = cabAut;
  cab[2] = NULL;
  return rede_baixar_st(completo, 25, cab, status);
}

char *nuvem_addon_alternar(const char *consulta, int ativo, const char *bearer, unsigned prazo_ms, int *status) {
  char completo[52000], cabApi[1300], cabAut[3100];
  const char *cab[5];
  const char *body = ativo ? "{\"enabled\":true}" : "{\"enabled\":false}";
#ifndef __EMSCRIPTEN__
  RedePedido p = {0};
  RedeResposta r = {0};
#endif
  if (status) *status = 0;
  if (!nuvem_pronta() || !bearer || !*bearer) return NULL;
  snprintf(completo, sizeof completo, "%s/rest/v1/addons?%s", url, consulta);
  snprintf(cabApi, sizeof cabApi, "apikey: %s", anon);
  snprintf(cabAut, sizeof cabAut, "Authorization: Bearer %s", bearer);
  cab[0] = cabApi; cab[1] = cabAut; cab[2] = "Content-Type: application/json";
  cab[3] = "Prefer: return=representation"; cab[4] = NULL;
#ifdef __EMSCRIPTEN__
  return rede_patch_st(completo, prazo_ms / 1000, cab, body, status);
#else
  p.metodo = "PATCH"; p.url = completo; p.cabecalhos = cab;
  p.corpo = body; p.n_corpo = strlen(body); p.prazo_ms = prazo_ms;
  rede_pedir(&p, &r);
  if (status) *status = r.status;
  char *out = r.corpo; r.corpo = NULL;
  rede_resposta_limpar(&r);
  return out;
#endif
}

int nuvem_erro_ausente(const char *corpoErro) {
  if (!corpoErro) return 0;
  return strstr(corpoErro, "PGRST202") != NULL ||
         strstr(corpoErro, "PGRST205") != NULL ||
         strstr(corpoErro, "Could not find the function") != NULL ||
         strstr(corpoErro, "Could not find the table") != NULL;
}

void nuvem_falhou(void) {
  int espera;
  if (falhas < 30) falhas++;
  espera = FREIO_MIN;
  { int i;
    for (i = 1; i < falhas && espera < FREIO_MAX; i++) espera *= 2; }
  if (espera > FREIO_MAX) espera = FREIO_MAX;
  freioAte = (long)time(NULL) + espera;
  printf("[nuvem] freio por %ds (falha %d)\n", espera, falhas);
}

void nuvem_ok(void) {
  falhas = 0;
  freioAte = 0;
}

int nuvem_freio_ativo(void) {
  return freioAte > (long)time(NULL);
}
