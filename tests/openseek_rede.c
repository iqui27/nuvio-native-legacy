// OPENSEEK (SR-3): o pino chega a libcurl como CURLOPT_PINNEDPUBLICKEY pelo
// caminho que FALHA FECHADO. Compila o rede_pedido.inc de verdade contra uma
// libcurl falsa que registra cada opcao: com pino a opcao 10230 e aplicada com
// o texto exato; se a libcurl recusa a opcao (sem suporte), o pedido falha
// ANTES de qualquer envio (perform nunca e chamado); pino diferente (curl 90)
// vira erro de transporte com curl_erro 90; sem pino a opcao nem e tocada (os
// outros chamadores de rede_pedir nao mudam).
//
//   bash tests/openseek_rede.sh
#include "../src/rede.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// As constantes de rede.c que o .inc usa.
#define OPT_URL             10002
#define OPT_WRITEFUNCTION   20011
#define OPT_WRITEDATA       10001
#define OPT_FOLLOWLOCATION     52
#define OPT_SSL_VERIFYPEER     64
#define OPT_SSL_VERIFYHOST     81
#define OPT_USERAGENT       10018
#define OPT_ACCEPT_ENCODING 10102
#define OPT_NOSIGNAL          99
#define OPT_HTTPHEADER      10023
#define OPT_NOBODY             44
#define OPT_POSTFIELDS      10015
#define OPT_CUSTOMREQUEST   10036
#define OPT_HEADERFUNCTION  20079
#define OPT_HEADERDATA      10029
#define OPT_TIMEOUT_MS        155
#define OPT_CONNECTTIMEOUT_MS 156
#define OPT_NOPROGRESS         43
#define OPT_XFERINFOFUNCTION 20219
#define OPT_XFERINFODATA     10057
#define OPT_CAINFO 10065
#define OPT_POSTFIELDSIZE 60
#define OPT_PROTOCOLS 181
#define INFO_URL_FINAL    1048577
#define INFO_RESPONSE_CODE   2097154
#define INFO_REDIRECT_URL 1048607
#define INFO_SIZE_DOWNLOAD 3145736

// --- libcurl falsa ---------------------------------------------------------------
static int nInit, nPerform, recusarPino, resultadoPerform, setouPino;
static long verifyPeer = -1, follow = -1;
static char pinoVisto[128];
typedef size_t (*Escrever)(void *, size_t, size_t, void *);
static Escrever escrever; static void *escreverDados;
static int fakeSetopt(void *c, int op, ...) {
  va_list a; (void)c;
  va_start(a, op);
  if (op == 10230) {
    const char *s = va_arg(a, const char *);
    setouPino++;
    snprintf(pinoVisto, sizeof pinoVisto, "%s", s ? s : "");
    va_end(a);
    return recusarPino;        // 4 = CURLE_NOT_BUILT_IN, 48 = UNKNOWN_OPTION
  }
  if (op == OPT_SSL_VERIFYPEER) verifyPeer = va_arg(a, long);
  else if (op == OPT_FOLLOWLOCATION) follow = va_arg(a, long);
  else if (op == OPT_WRITEFUNCTION) escrever = va_arg(a, Escrever);
  else if (op == OPT_WRITEDATA) escreverDados = va_arg(a, void *);
  va_end(a);
  return 0;
}
#define curl_setopt(c, op, v) fakeSetopt((c), (op), (v))
static int dummy;
static void *curl_init(void) { nInit++; return &dummy; }
static void curl_cleanup_f(void *c) { (void)c; }
static void (*curl_cleanup)(void *) = curl_cleanup_f;
static int curl_getinfo_f(void *c, int info, ...) {
  va_list a; (void)c;
  va_start(a, info);
  if (info == INFO_RESPONSE_CODE) *va_arg(a, long *) = resultadoPerform ? 0 : 200;
  else if (info == INFO_SIZE_DOWNLOAD) *va_arg(a, double *) = 2;
  else if (info == INFO_URL_FINAL || info == INFO_REDIRECT_URL) *va_arg(a, char **) = NULL;
  va_end(a);
  return 0;
}
static int (*curl_getinfo)(void *, int, ...) = curl_getinfo_f;
static void *slist_append_f(void *l, const char *s) { (void)s; return l ? l : &dummy; }

static void *(*slist_append)(void *, const char *) = slist_append_f;
static void slist_free_f(void *l) { (void)l; }
static void (*slist_free)(void *) = slist_free_f;
static int performTmdb(void *c, const char *url) {
  (void)url; nPerform++;
  if (!resultadoPerform && escrever) escrever("{}", 1, 2, escreverDados);
  (void)c;
  return resultadoPerform;     // 90 = CURLE_SSL_PINNEDPUBKEYNOTMATCH
}
static int abrir(void) { return 1; }
static char discordCa[600];
static uint64_t pedidoAgoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (unsigned long)t.tv_nsec / 1000000u;
}
static int retrySegundos(const char *s, size_t n) { (void)s; (void)n; return 0; }
void rede_job_reter(RedeJob *j) { (void)j; }
void rede_job_soltar(RedeJob *j) { (void)j; }
RedeErro rede_job_estado(RedeJob *j) { (void)j; return REDE_OK; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); memset(r, 0, sizeof *r); }

#include "../src/rede_pedido.inc"

#define PINO "sha256//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
static void zerarFalsa(void) {
  nInit = nPerform = recusarPino = resultadoPerform = setouPino = 0;
  verifyPeer = follow = -1; pinoVisto[0] = 0; escrever = NULL;
}

int main(void) {
  RedePedido p;
  RedeResposta r;


  // 1. Com pino: aplicado, com o texto exato, servidor pessoal (sem CA), sem redirect.
  zerarFalsa();
  memset(&p, 0, sizeof p);
  p.metodo = "POST"; p.url = "https://nas.local:8787/v1/previews"; p.corpo = "{}"; p.n_corpo = 2;
  p.tls_pessoal = 1; p.seguir = 0; p.pino_publico = PINO; p.prazo_ms = 5000;
  assert(rede_pedir(&p, &r) && r.status == 200 && r.erro == REDE_OK);
  assert(setouPino == 1 && !strcmp(pinoVisto, PINO) && nPerform == 1);
  assert(verifyPeer == 0 && follow == 0);   // o pino segue valendo com VERIFYPEER 0
  rede_resposta_limpar(&r);

  // 2. libcurl sem CURLOPT_PINNEDPUBLICKEY: falha fechado, nada sai.
  for (int e = 0; e < 2; e++) {
    zerarFalsa(); recusarPino = e ? 48 : 4;
    assert(!rede_pedir(&p, &r));
    assert(r.erro == REDE_INDISPONIVEL && r.curl_erro == recusarPino && nPerform == 0 && !r.corpo);
    rede_resposta_limpar(&r);
  }

  // 3. Certificado trocado: curl 90 -> transporte, curl_erro 90, sem corpo.
  zerarFalsa(); resultadoPerform = REDE_CURL_PINO_DIFERENTE;
  assert(!rede_pedir(&p, &r) && r.erro == REDE_TRANSPORTE && r.curl_erro == 90 && !r.corpo);
  rede_resposta_limpar(&r);

  // 4. Pino com http://, ou pino que nao e "sha256//..." (um arquivo): entrada recusada
  //    antes de abrir a libcurl.
  zerarFalsa(); p.url = "http://nas.local:8787/v1/previews";
  assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA && nInit == 0);
  zerarFalsa(); p.url = "https://nas.local:8787/v1/previews"; p.pino_publico = "/sdcard/pin.der";
  assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA && nInit == 0);
  zerarFalsa(); p.pino_publico = "sha256//curto=";
  assert(!rede_pedir(&p, &r) && r.erro == REDE_ENTRADA && nInit == 0);

  // 5. Sem pino (todos os chamadores de antes): a opcao 10230 nem e tocada.
  zerarFalsa(); p.pino_publico = NULL; p.tls_pessoal = 0;
  assert(rede_pedir(&p, &r) && setouPino == 0 && verifyPeer == 1);
  rede_resposta_limpar(&r);

  printf("openseek_rede: ok\n");
  return 0;
}
