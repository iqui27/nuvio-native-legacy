// Exercise the production asynchronous lookup/refresh path with deterministic
// HTTP responses. The ledger uses real atomic files/fsync; no external API.
#include "dados.h"
#include "nvtmp.h"
#include "rede.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
int perftv_enabled, perftv_uploads;
#include <stdlib.h>
#include <string.h>
#include <time.h>
static time_t agora = 2000000000;
static time_t tempo(time_t *p) { if (p) *p = agora; return agora; }
#define time tempo
#include "../src/seekr.c"
#undef time

// Ajustes nao entra nesta bancada: relogio de 12 h desligado (tests/guia_logos.c).
int ajustes_relogio_12h(void) { return 0; }

void gfx_tex_esquecer(GLuint t) { (void)t; }
static pthread_mutex_t redeTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t redeCond = PTHREAD_COND_INITIALIZER;
static int spriteN, vttN, sheetN, validationN, segurar;
static int respostas[4], nRespostas, respostaIdx;
static int vttHttp = 200, sheetHttp = 200, retryDepois = 120;
static const char *esperada = "personal-test-key";

static void respostasHttp(int a, int b) {
  respostas[0] = a; respostas[1] = b;
  nRespostas = b ? 2 : 1; respostaIdx = 0;
}
static int usadas(void) {
  SeekrQuotaUso u; seekrquota_uso(agora, &u); return u.usadas;
}
char *rede_baixar_st_retry(const char *u, int s, const char *const *cab,
                           int *st, int *retry) {
  (void)s;
  assert(strstr(u, "https://api.seekr.tv/sprites?"));
  assert(cab && cab[0] && !strncmp(cab[0], "X-API-Key: ", 11));
  assert(!strcmp(cab[0] + 11, esperada));
  // The durable ledger MUST precede dispatch, rather than just the UI counter.
  char *ledger = dados_ler("seekrquota-v1.txt");
  assert(ledger); int ver, count; long long dia, ultimo;
  assert(sscanf(ledger, "%d %lld %lld %d", &ver, &dia, &ultimo, &count) == 4 && count > 0);
  free(ledger);
  pthread_mutex_lock(&redeTrava);
  spriteN++;
  while (segurar) pthread_cond_wait(&redeCond, &redeTrava);
  int r = respostaIdx < nRespostas ? respostas[respostaIdx++] : 200;
  pthread_mutex_unlock(&redeTrava);
  *st = r; *retry = r == 429 ? retryDepois : 0;
  if (r == 0) return NULL;
  if (r != 200) return strdup("{\"error\":\"fixture\"}");
  char json[500];
  snprintf(json, sizeof json,
    "{\"vtt_url\":\"https://sprites.seekr.tv/preview.vtt?exp=%lld&sig=redacted\"}",
    (long long)agora + 3600);
  return strdup(json);
}
char *rede_baixar_st(const char *u, int s, const char *const *cab, int *st) {
  (void)s;
  if (strstr(u, "/keys/validate")) {
    assert(cab && cab[0]); validationN++; *st = 200;
    return strdup("{ \"valid\": true }");
  }
  assert(!cab); // Signed hops never expose the key.
  assert(strstr(u, "https://sprites.seekr.tv/"));
  vttN++; *st = vttHttp;
  if (vttHttp != 200) { vttHttp = 200; return strdup("expired"); }
  char vttTexto[600];
  snprintf(vttTexto, sizeof vttTexto,
    "WEBVTT\n\n00:00:00.000 --> 00:00:10.000\n"
    "https://sprites.seekr.tv/sheet.jpg?exp=%lld&sig=redacted#xywh=0,0,320,180\n",
    (long long)agora + 3600);
  return strdup(vttTexto);
}
char *rede_baixar_bin_medido_controle(const char *u, int s, const char *const *cab,
                                      const RedeControle *controle, long *n, RedeMedida *medida) {
  (void)u; (void)s; (void)controle; assert(!cab);
  sheetN++; memset(medida, 0, sizeof *medida); medida->status = sheetHttp;
  if (sheetHttp != 200) { *n = 0; return NULL; }
  FILE *f = fopen("tests/amostra.jpg", "rb"); assert(f);
  fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
  char *b = malloc((size_t)*n); assert(b && fread(b, 1, (size_t)*n, f) == (size_t)*n);
  fclose(f); return b;
}
static void terminou(int est) {
  for (int i = 0; i < 1500; i++) {
    pthread_mutex_lock(&trava);
    int pendentes = 0;
    for (int j = 0; j < SK_LOOKUPS; j++) pendentes += consultas[j] != NULL;
    pthread_mutex_unlock(&trava);
    if (!pendentes) { assert(seekr_estado() == est); return; }
    SDL_Delay(2);
  }
  assert(!"lookup timed out");
}
static void pedir(int est) { seekr_pedir("tt0133093", 0, 0, 7200000); terminou(est); }
static void recorte(void) {
  Recorte *r = calloc(1, sizeof *r); assert(r);
  snprintf(r->url, sizeof r->url, "%s", vtt.folhas[0]);
  r->folha = 0; r->x = 0; r->y = 0; r->w = 320; r->h = 180; r->g = geracao;
  emVoo = 1; recortar(r);
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(nvt(""));
  // Sem chave: nem a validacao nem a miniatura chegam a rede.
  assert(seekr_validar("") == 0);
  seekr_pedir("tt0133093", 0, 0, 7200000);
  assert(validationN == 0 && spriteN == 0 && usadas() == 0);
  seekr_definir_chave(esperada);
  assert(seekr_validar(esperada) == 1 && validationN == 1 && usadas() == 0);
  // Equal requests in flight have one reservation and one dispatch.
  segurar = 1; seekr_pedir("tt0133093", 0, 0, 7200000);
  for (int i = 0; i < 1000; i++) {
    pthread_mutex_lock(&redeTrava); int n = spriteN; pthread_mutex_unlock(&redeTrava);
    if (n) break; SDL_Delay(1);
  }
  for (int i = 0; i < 30; i++) seekr_pedir("tt0133093", 0, 0, 7200000 + i * 10);
  pthread_mutex_lock(&redeTrava); assert(spriteN == 1); segurar = 0;
  pthread_cond_broadcast(&redeCond); pthread_mutex_unlock(&redeTrava);
  terminou(SEEKR_PRONTO); assert(usadas() == 1 && vttN == 1);
  pedir(SEEKR_PRONTO); assert(spriteN == 1 && usadas() == 1);
  recorte(); assert(sheetN == 1 && usadas() == 1 && jpgBytes);
  seekr_ocioso(); recorte(); assert(sheetN == 1 && usadas() == 1);
  // Profile/account changes cannot reset the installation-wide budget.
  seekr_desligar(); pedir(SEEKR_PRONTO); assert(usadas() == 2);
  esperada = "another-personal-key"; seekr_definir_chave(esperada);
  pedir(SEEKR_PRONTO); assert(usadas() == 3);
  // Transient retry reserves a second call; exhausted retries are distinct.
  seekr_tentar_novamente(); respostasHttp(0, 200); pedir(SEEKR_PRONTO); assert(usadas() == 5);
  seekr_tentar_novamente(); respostasHttp(500, 500); pedir(SEEKR_REDE_INDISPONIVEL); assert(usadas() == 7);
  agora += 31; respostasHttp(200, 0); pedir(SEEKR_PRONTO); assert(usadas() == 8);
  seekr_tentar_novamente(); respostasHttp(401, 0); pedir(SEEKR_CHAVE_RECUSADA); assert(usadas() == 9);
  seekr_tentar_novamente(); respostasHttp(403, 0); pedir(SEEKR_CHAVE_RECUSADA); assert(usadas() == 10);
  seekr_tentar_novamente(); respostasHttp(404, 0); pedir(SEEKR_SEM_PREVIA); assert(usadas() == 11);
  seekr_tentar_novamente(); respostasHttp(429, 0); pedir(SEEKR_LIMITE_PROVEDOR); assert(usadas() == 12);
  SeekrUso u; seekr_uso(&u); assert(u.retryUtc == agora + retryDepois && u.http == 429);
  int antes = spriteN; seekr_tentar_novamente(); pedir(SEEKR_LIMITE_PROVEDOR);
  assert(spriteN == antes && usadas() == 12);
  agora += retryDepois; respostasHttp(200, 0); pedir(SEEKR_PRONTO); assert(usadas() == 13);
  // Signed VTT expiration refreshes once through /sprites, never for free.
  seekr_tentar_novamente(); vttHttp = 403; pedir(SEEKR_PRONTO); assert(usadas() == 15);
  agora += 3601; pedir(SEEKR_PRONTO); assert(usadas() == 16);
  // Expired JPEG signatures also invalidate lookup and refresh through quota.
  sheetHttp = 403; recorte(); assert(expiraUtc == 0 && usadas() == 16);
  sheetHttp = 200; pedir(SEEKR_PRONTO); assert(usadas() == 17);
  while (usadas() < 50) { seekr_tentar_novamente(); pedir(SEEKR_PRONTO); }
  antes = spriteN; seekr_tentar_novamente(); pedir(SEEKR_LIMITE_LOCAL);
  assert(spriteN == antes && usadas() == 50);
  esperada = "third-key"; seekr_definir_chave(esperada); pedir(SEEKR_LIMITE_LOCAL);
  assert(spriteN == antes && usadas() == 50);
  assert(seekr_validar(esperada) == 1 && usadas() == 50);
  // #312: chave pessoal nao paga nem sofre o teto local de 50 (o plano e do Seekr).
  seekr_chave_propria(1); esperada = "own-key"; seekr_definir_chave(esperada);
  seekr_tentar_novamente(); respostasHttp(200, 0); pedir(SEEKR_PRONTO);
  assert(usadas() == 50 && spriteN > antes);
  { SeekrUso p; seekr_uso(&p); assert(p.chavePropria); }
  seekr_chave_propria(0);
  seekr_desligar();
  puts("seekrquota pipeline: PASS single-flight, durable-before-HTTP, cache, key/profile changes, retries, expiry, states and50limit");
  return 0;
}
