// Pedido que leva credencial verifica o TLS e nao segue redirect; a midia publica
// dos addons (GET sem cabecalho de credencial) continua como sempre. Usa a
// implementacao real de rede.c com uma libcurl falsa que registra as opcoes no
// momento do perform.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rede.c"
typedef struct { long peer, host, follow; int pPeer, pHost, pFollow; } Fake;
static Fake ultimo;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static int option(void *c, int opt, ...) {
  Fake *f = c; va_list ap; va_start(ap, opt);
  if (opt == OPT_SSL_VERIFYPEER) { f->peer = va_arg(ap, long); f->pPeer = 1; }
  else if (opt == OPT_SSL_VERIFYHOST) { f->host = va_arg(ap, long); f->pHost = 1; }
  else if (opt == OPT_FOLLOWLOCATION) { f->follow = va_arg(ap, long); f->pFollow = 1; }
  va_end(ap); return 0;
}
static int perform(void *c) { ultimo = *(Fake *)c; return 0; }
static int info(void *c, int opt, ...) {
  (void)c; va_list ap; va_start(ap, opt);
  if (opt == INFO_RESPONSE_CODE) *va_arg(ap, long *) = 200;
  va_end(ap); return 0;
}
static void cleanup(void *c) { free(c); }
static void *slAppend(void *l, const char *s) { (void)s; return l ? l : (void *)1; }
static void slFree(void *l) { (void)l; }

int main(void) {
  int st; char *b;
  const char *cabAut[] = { "Authorization: Bearer x", NULL };
  const char *cabKey[] = { "apikey: k", "Accept: */*", NULL };
  const char *cabNeutro[] = { "Accept: */*", "Range: bytes=0-9", NULL };
  pronto = 1; curl_init = init; curl_setopt_f = option; curl_perform = perform;
  curl_cleanup = cleanup; curl_getinfo = info; slist_append = slAppend; slist_free = slFree;

  // POST de API (login/token/sync): verificado.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_postar_st("https://api.example.test/token", 5, cabAut, "{}", &st);
  assert(b && st == 200); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2);

  // GET com credencial: verificado e SEM redirect.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://api.example.test/me", 5, cabAut); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://api.example.test/me", 5, cabKey); free(b);
  assert(ultimo.peer == 1 && ultimo.follow == 0);

  // GET sem credencial (midia/imagem publica dos addons): comportamento de sempre.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://cdn.example.test/img.jpg", 5, cabNeutro); free(b);
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar("https://cdn.example.test/img.jpg", 5); free(b);
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);

  // Credencial em http:// nao muda nada aqui (nao ha TLS a verificar); segue como era.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("http://127.0.0.1/x", 5, cabAut); free(b);
  assert(ultimo.peer == 0);
  puts("rede_tls_credencial: ok");
  return 0;
}
