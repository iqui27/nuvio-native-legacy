// #764: cache USB de links diretos usa pasta wa no pendrive.
#include "p2pmotor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int usbOn;
int p2pmotor_usb_presente(void) { return usbOn; }
void p2pmotor_usb_info(P2pmUsbInfo *o) {
  memset(o, 0, sizeof *o);
  o->estado = 1; o->gravavel = 1;
  snprintf(o->caminho, sizeof o->caminho, "/tmp/usb-nuvio");
}
int ajustes_direct_cache_usb(void) { return usbOn; }
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                        int (*cb)(const char *dados, size_t n, void *u), void *u,
                        long long maxBytes, int *status, long long *bytes) {
  (void)url; (void)segundos; (void)cab; (void)cb; (void)u; (void)maxBytes; (void)status; (void)bytes; return 0; }
#include "../src/directcache.c"
int main(void) {
  char dir[256], buf[512];
  usbOn = 0;
  assert(!directcache_ativo());
  assert(!directcache_pasta(dir, sizeof dir));
  usbOn = 1;
  assert(directcache_ativo());
  assert(directcache_pasta(dir, sizeof dir));
  assert(strstr(dir, "/wa"));
  assert(!strcmp(directcache_preparar("https://cdn.example/v.mp4", NULL, buf, sizeof buf),
                 "https://cdn.example/v.mp4"));
  puts("d1_direct_cache_usb: ok");
  return 0;
}
