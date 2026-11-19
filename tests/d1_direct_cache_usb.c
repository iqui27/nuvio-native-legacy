// #764: cache USB de links diretos usa pasta wa no pendrive.
#include "p2pmotor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static char usbRoot[512];
static int usbOn;

int p2pmotor_usb_presente(void) { return usbOn; }
void p2pmotor_usb_info(P2pmUsbInfo *o) {
  memset(o, 0, sizeof *o);
  o->estado = 1; o->gravavel = 1;
  snprintf(o->caminho, sizeof o->caminho, "%s", usbRoot);
}
int ajustes_direct_cache_usb(void) { return usbOn; }
unsigned ajustes_p2p_limite_mb(void) { return 512; }
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                        int (*cb)(const char *dados, size_t n, void *u), void *u,
                        long long maxBytes, int *status, long long *bytes) {
  (void)url; (void)segundos; (void)cab; (void)cb; (void)u; (void)maxBytes; (void)status; (void)bytes; return 0; }
#include "../src/directcache.c"

static void mkusb(void) {
  const char *base = getenv("TMPDIR");
  if (!base || !base[0]) base = "/home/ubuntu/nuvio-qa-21/global/runs/composer-f11-tmp";
  snprintf(usbRoot, sizeof usbRoot, "%s/d1-dc-usb-%d", base, (int)getpid());
  mkdir(base, 0755);
  mkdir(usbRoot, 0755);
}

int main(void) {
  char dir[256], buf[512];
  mkusb();
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
