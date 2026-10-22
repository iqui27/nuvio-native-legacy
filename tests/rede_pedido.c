#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef NV_TPK40
void nv_tpk40_etapa(const char *s) { (void)s; }
#endif
static const char *base, *tlsBase, *ca;
static char url[4096];
static const char *cab[] = {"Authorization: Bearer synthetic", "Cookie: synthetic=1", "X-Private-Key: synthetic", NULL};
static RedePedido pedido(const char *rota) {
  RedePedido p = {0};
  snprintf(url, sizeof url, "%s%s", base, rota);
  p.url = url; p.prazo_ms = 5000; p.seguir = 1;
  return p;
}
static unsigned nAmostras, completas, zero, nested;
static uint64_t bytesAmostras;
static unsigned msAmostras;
static void amostra(const RedeIntervalo *v, void *u) {
  (void)u;
  nAmostras++; completas += v->completa; bytesAmostras += v->bytes;
  msAmostras += v->ms;
  if (!v->bytes && v->completa) zero++;
  assert(v->ms > 0);
  assert(v->completa == (v->ms >= 1000));
}
static void nestedAmostra(const RedeIntervalo *v, void *u) {
  RedeResposta r;
  RedePedido p = {0};
  (void)v;
  if (nested++) return;
  p.url = u;
  assert(rede_pedir(&p, &r));
  assert(r.n_corpo == 7 && !memcmp(r.corpo, "healthy", 7));
  rede_resposta_limpar(&r);
}
static void invalidateFinal(const RedeIntervalo *v, void *u) {
  assert(!v->completa);
  rede_grupo_avancar(u);
}
typedef struct {
  RedeJob *job;
  RedeGrupo *grupo;
  int geracao;
  unsigned delay;
} Corta;
static void *cortar(void *u) {
  Corta *c = u;
  usleep(c->delay * 1000u);
  if (c->geracao) rede_grupo_avancar(c->grupo);
  else rede_job_cancelar(c->job);
  return NULL;
}
static void verificaCorte(int geracao) {
  RedeGrupo *g = rede_grupo_criar();
  RedeJob *j = rede_job_criar(g), *outro = rede_job_criar(g);
  RedePedido p = pedido("/stall");
  RedeResposta r;
  Corta c = {j, g, geracao, 180};
  pthread_t t;
  assert(g && j && outro);
  p.job = j;
  assert(!pthread_create(&t, NULL, cortar, &c));
  assert(!rede_pedir(&p, &r));
  pthread_join(t, NULL);
  assert(r.erro == (geracao ? REDE_GERACAO : REDE_CANCELADO));
  assert(!r.corpo && r.n_corpo == 0 && r.ms < 2500);
  assert(rede_job_estado(outro) == (geracao ? REDE_GERACAO : REDE_OK));
  rede_resposta_limpar(&r);
  if (geracao) {
    RedeJob *novo = rede_job_criar(g);
    p = pedido("/ok"); p.job = novo;
    assert(rede_pedir(&p, &r));
    rede_resposta_limpar(&r); rede_job_soltar(novo);
  }
  rede_grupo_soltar(g); /* jobs still retain it */
  assert(rede_job_estado(j) != REDE_OK);
  rede_job_soltar(j); rede_job_soltar(outro);
}
static void *paralelo(void *u) {
  size_t cap = (size_t)u;
  char destino[4096];
  RedePedido p = {0}; RedeResposta r;
  snprintf(destino, sizeof destino, "%s/large-chunk", base);
  p.url = destino; p.max_bytes = cap;
  assert(!rede_pedir(&p, &r));
  assert(r.erro == REDE_LIMITE_CORPO && !r.corpo);
  rede_resposta_limpar(&r);
  snprintf(destino, sizeof destino, "%s/ok", base);
  p.max_bytes = 0;
  assert(rede_pedir(&p, &r) && r.n_corpo == 7);
  rede_resposta_limpar(&r);
  return NULL;
}
static void *coldStart(void *u) {
  char destino[4096]; RedePedido p = {0}; RedeResposta r;
  (void)u;
  snprintf(destino, sizeof destino, "%s/ok", base); p.url = destino;
  assert(rede_pedir(&p, &r) && r.n_corpo == 7);
  rede_resposta_limpar(&r); return NULL;
}
int main(int argc, char **argv) {
  RedePedido p;
  RedeResposta r;
  pthread_t t[4];
  unsigned k;
  assert(argc == 4); base = argv[1]; tlsBase = argv[2]; ca = argv[3];
  assert(rede_pedido_capacidades() & REDE_CAP_JOB);
  for (k = 0; k < 4; k++) assert(!pthread_create(&t[k], NULL, coldStart, NULL));
  for (k = 0; k < 4; k++) pthread_join(t[k], NULL);
  p = pedido("/ok");
  assert(rede_pedir(&p, &r) && r.status == 200 && !r.erro);
  assert(r.n_corpo == 7 && !memcmp(r.corpo, "healthy", 7));
  assert(!strcmp(r.mime, "application/json"));
  assert(r.n_cabecalhos > 0 && r.bytes_fio == 7 && !r.intervalos_completos);
  assert(strstr(r.host, "/...") && !strstr(r.host, "/ok"));
  rede_resposta_limpar(&r);
  p = pedido("/bin");
  assert(rede_pedir(&p, &r) && r.n_corpo == 4 && !memcmp(r.corpo, "a\0b\0", 4));
  rede_resposta_limpar(&r);
  p = pedido("/error");
  assert(rede_pedir(&p, &r) && r.status == 503 && !r.erro && r.corpo);
  rede_resposta_limpar(&r);
  p = pedido("/retry-seconds");
  assert(rede_pedir(&p, &r) && r.status == 429 && r.retry_after_s == 19);
  rede_resposta_limpar(&r);
  p = pedido("/retry-date");
  assert(rede_pedir(&p, &r) && r.retry_after_s >= 85 && r.retry_after_s <= 90);
  rede_resposta_limpar(&r);
  p = pedido("/same"); p.cabecalhos = cab;
  assert(rede_pedir(&p, &r) && strstr(r.corpo, "\"private\": 1"));
  assert(strstr(r.final, "/echo") && !r.retry_after_s);
  rede_resposta_limpar(&r);
  p = pedido("/cross"); p.cabecalhos = cab;
  assert(rede_pedir(&p, &r) && strstr(r.corpo, "\"present\": 0"));
  rede_resposta_limpar(&r);
  p = pedido("/back"); p.cabecalhos = cab;
  assert(rede_pedir(&p, &r) && strstr(r.corpo, "\"present\": 0"));
  rede_resposta_limpar(&r);
  p = pedido("/cross303"); p.metodo = "POST"; p.corpo = "hello"; p.n_corpo = 5; p.cabecalhos = cab;
  assert(rede_pedir(&p, &r) && strstr(r.corpo, "\"method\": \"GET\"") && strstr(r.corpo, "\"body\": 0"));
  rede_resposta_limpar(&r);
  p = pedido("/cross307"); p.metodo = "POST"; p.corpo = "hello"; p.n_corpo = 5; p.cabecalhos = cab;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_REDIRECT && !r.corpo);
  rede_resposta_limpar(&r);
  p = pedido("/same"); p.seguir = 0;
  assert(rede_pedir(&p, &r) && r.status == 302);
  rede_resposta_limpar(&r);
  p = pedido("/loop");
  assert(!rede_pedir(&p, &r) && r.erro == REDE_REDIRECT);
  rede_resposta_limpar(&r);
  p = pedido("/file");
  assert(!rede_pedir(&p, &r) && r.erro == REDE_REDIRECT);
  rede_resposta_limpar(&r);
  p = pedido("/large-length"); p.max_bytes = 127;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_LIMITE_CORPO && !r.corpo);
  rede_resposta_limpar(&r);
  p = pedido("/large-length"); p.metodo = "HEAD"; p.max_bytes = 127;
  assert(rede_pedir(&p, &r) && r.status == 200 && !r.n_corpo);
  rede_resposta_limpar(&r);
  p = pedido("/large-chunk"); p.max_bytes = 127;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_LIMITE_CORPO && !r.corpo);
  rede_resposta_limpar(&r);
  p = pedido("/gzip"); p.max_bytes = 8192;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_LIMITE_CORPO && !r.corpo);
  rede_resposta_limpar(&r);
  p.max_bytes = 1024 * 1024;
  assert(rede_pedir(&p, &r) && r.n_corpo == 512 * 1024 && r.bytes_fio < 4096);
  rede_resposta_limpar(&r);
  p = pedido("/headers"); p.max_cabecalhos = 200;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_LIMITE_CABECALHOS && !r.corpo);
  assert(r.n_cabecalhos <= 200); rede_resposta_limpar(&r);
  p = pedido("/slowheaders"); p.prazo_ms = 200;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_PRAZO && r.ms >= 170 && r.ms < 1500);
  rede_resposta_limpar(&r);
  p = pedido("/redirectslow"); p.prazo_ms = 450;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_PRAZO && r.ms >= 400 && r.ms < 1500);
  rede_resposta_limpar(&r);
  p = pedido("/truncated");
  assert(!rede_pedir(&p, &r) && r.erro == REDE_TRANSPORTE && !r.corpo);
  rede_resposta_limpar(&r);
  verificaCorte(0); verificaCorte(1);
  {
    RedeGrupo *g = rede_grupo_criar(); RedeJob *j = rede_job_criar(g);
    p = pedido("/late"); p.job = j; p.intervalo = invalidateFinal; p.intervalo_usuario = g;
    assert(!rede_pedir(&p, &r) && r.erro == REDE_GERACAO && !r.corpo);
    rede_resposta_limpar(&r); rede_job_soltar(j);
    j = rede_job_criar(g); p = pedido("/ok"); p.job = j;
    assert(rede_pedir(&p, &r));
    rede_grupo_avancar(g); assert(rede_job_estado(j) == REDE_GERACAO); /* UI fence after return */
    rede_resposta_limpar(&r); rede_job_soltar(j); rede_grupo_soltar(g);
  }
  {
    RedeGrupo *g = rede_grupo_criar(); RedeJob *j = rede_job_criar(g);
    rede_grupo_cancelar(g);
    p = pedido("/ok"); p.job = j;
    assert(!rede_pedir(&p, &r) && r.erro == REDE_CANCELADO && !r.status);
    rede_resposta_limpar(&r); rede_grupo_soltar(g); rede_job_soltar(j);
  }
  for (k = 0; k < 4; k++) assert(!pthread_create(&t[k], NULL, paralelo, (void *)(size_t)(16 + k * 2048)));
  for (k = 0; k < 4; k++) pthread_join(t[k], NULL);
  p = pedido("/stream"); p.intervalo = amostra;
  assert(rede_pedir(&p, &r) && r.bytes_fio == 4 * 4096 && r.n_corpo == 4 * 4096);
  assert(nAmostras >= 3 && completas >= 2 && zero >= 1);
  assert(bytesAmostras == r.bytes_fio && msAmostras == r.corpo_ms);
  assert(r.intervalos_completos == completas && r.corpo_ms >= 3200);
  rede_resposta_limpar(&r);
  {
    char dest[4096]; snprintf(dest, sizeof dest, "%s/ok", base);
    p = pedido("/stream"); p.intervalo = nestedAmostra; p.intervalo_usuario = dest;
    assert(rede_pedir(&p, &r) && nested >= 1 && r.n_corpo == 4 * 4096);
    rede_resposta_limpar(&r);
  }
  p = pedido("/ok"); p.url = "file:///does-not-exist";
  assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA); rede_resposta_limpar(&r);
  p.url = "http://synthetic@localhost/";
  assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA); rede_resposta_limpar(&r);
  p = pedido("/ok");
  { const char *bad[] = {"X-Header: value\r\nAuthorization: synthetic", NULL}; p.cabecalhos = bad;
    assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA); rede_resposta_limpar(&r); }
  snprintf(url, sizeof url, "%s/ok", tlsBase); p = (RedePedido){0}; p.url = url;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_TRANSPORTE && !r.corpo); rede_resposta_limpar(&r);
  p.ca_arquivo = ca;
  assert(rede_pedir(&p, &r) && r.status == 200); rede_resposta_limpar(&r);
  rede_discord_ca(ca); p.ca_arquivo = NULL;
  assert(rede_pedir(&p, &r) && r.status == 200); rede_resposta_limpar(&r);
  rede_discord_ca(NULL);
  /* #290: personal server, self-signed: no CA, only its own origin skips verification. */
  snprintf(url, sizeof url, "%s/ok", tlsBase); p = (RedePedido){0}; p.url = url; p.tls_pessoal = 1;
  assert(rede_pedir(&p, &r) && r.status == 200); rede_resposta_limpar(&r);
  snprintf(url, sizeof url, "%s/tlsother", tlsBase); p.seguir = 1;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_TRANSPORTE); rede_resposta_limpar(&r);
  p.ca_arquivo = ca;
  snprintf(url, sizeof url, "%s/downgrade", tlsBase); p.seguir = 1;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_REDIRECT); rede_resposta_limpar(&r);
  {
    int status, retry; char *b;
    p = pedido("/retry-date");
    b = rede_baixar_st_retry(p.url, 5, NULL, &status, &retry);
    assert(b && status == 429 && retry >= 85 && retry <= 90); free(b);
  }
  puts("rede_pedido: PASS (local HTTP/TLS, limits, generation, cancel, deadline, redirects, wire intervals)");
  return 0;
}
