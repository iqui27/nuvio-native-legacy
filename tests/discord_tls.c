// Assert OAuth/Gateway options using the actual network implementation.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rede.c"
typedef struct { long peer, host, redirects; const char *ca; } Fake;
static int cleaned, performed, transportError, attempts;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static int option(void *c, int opt, ...) {
  Fake *f = c; va_list ap; va_start(ap, opt);
  if (opt == OPT_SSL_VERIFYPEER) f->peer = va_arg(ap, long);
  else if (opt == OPT_SSL_VERIFYHOST) f->host = va_arg(ap, long);
  else if (opt == OPT_FOLLOWLOCATION) f->redirects = va_arg(ap, long);
  else if (opt == OPT_CAINFO) f->ca = va_arg(ap, const char *);
  va_end(ap); return 0;
}
static int perform(void *c) {
  Fake *f = c; performed++;
  assert(f->peer == 1 && f->host == 2 && f->redirects == 0);
  if (discordCa[0]) assert(f->ca && !strcmp(f->ca, discordCa));
  else assert(!f->ca);
  return transportError;
}
static int info(void *c, int opt, ...) {
  (void)c; va_list ap; va_start(ap,opt);
  if (opt == INFO_LASTSOCKET) *va_arg(ap,long *) = 5;
  else if (opt == INFO_RESPONSE_CODE) *va_arg(ap,long *) = 200;
  va_end(ap); return 0;
}
static void cleanup(void *c) { cleaned++; free(c); }
static int sendOnce(void *c, const void *b, size_t n, size_t *sent) {
  (void)c; (void)b; (void)n; attempts++; *sent = 0; return CURLE_AGAIN_;
}
static int recvOnce(void *c, void *b, size_t n, size_t *got) {
  (void)c; (void)b; (void)n; *got = 0; return CURLE_AGAIN_;
}
int main(void) {
  int st; char *b; RedeTls *t; size_t n = 99;
  pronto=1; curl_init=init; curl_setopt_f=option; curl_perform=perform;
  curl_cleanup=cleanup; curl_getinfo=info; curl_send=sendOnce; curl_recv=recvOnce;
  assert(!rede_postar_seguro_st("http://discord.com/api/test",1,NULL,"fixture",&st));
  assert(st==0 && performed==0);
  rede_discord_ca("/fixture/discord-ca.pem");
  b=rede_postar_seguro_st("https://discord.com/api/test",1,NULL,"fixture",&st);
  assert(b && st==200); free(b);
  t=rede_tls_abrir("https://gateway.discord.gg/",1); assert(t);
  assert(rede_tls_tentar_enviar(t,"fixture",7,&n)==0 && n==0 && attempts==1);
  rede_tls_fechar(t);
  // Missing/invalid trust must fail once; there is no insecure retry.
  transportError=77; int before=performed;
  assert(!rede_postar_seguro_st("https://discord.com/api/test",1,NULL,"fixture",&st));
  assert(st==0 && performed==before+1);
  assert(!rede_tls_abrir("https://gateway.discord.gg/",1));
  assert(performed==before+2);
  transportError=0; rede_discord_ca("");
  b=rede_postar_seguro_st("https://discord.com/api/test",1,NULL,"fixture",&st);
  assert(b && st==200); free(b);
  assert(cleaned==5); puts("discord_tls: ok"); return 0;
}
