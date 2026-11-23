#define _POSIX_C_SOURCE 200809L
#include "rotahost.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *disco;
static char *ler(const char *nome) {
  assert(!strcmp(nome, "rotahost.txt"));
  return disco ? strdup(disco) : NULL;
}
static int gravar(const char *nome, const char *buf) {
  assert(!strcmp(nome, "rotahost.txt"));
  free(disco); disco = strdup(buf); assert(disco); return 1;
}
static int apagar(const char *nome) {
  assert(!strcmp(nome, "rotahost.txt"));
  free(disco); disco = NULL; return 1;
}
static void recarregar(void) { rotahost_definir_arquivo(ler, gravar, apagar); }
static void par(RotaTentativa *p, int i, int modo, int rota) {
  assert(p[i].modo == modo && p[i].rota == rota);
}
int main(void) {
  RotaTentativa p[8];
  RotaPlanoEntrada e = {"https://exemplo:80/live?token=x", 1, -1, 1, -1, 0, 1};
  recarregar(); rotahost_limpar();
  assert(rotahost_rota(NULL) == -1 && rotahost_modo("") == -1);
  rotahost_funcionou("nvproxy:https://User:Password@EXEMPLO:80/path?secret=1", 0, 2);
  assert(rotahost_rota(e.url) == 0 && rotahost_modo(e.url) == 2);
  assert(strstr(disco, "exemplo:80\t0\t2\t0\t0\t"));
  assert(!strstr(disco, "User") && !strstr(disco, "Password") && !strstr(disco, "path") && !strstr(disco, "secret"));
  assert(rotahost_rota("https://exemplo:81/live") == -1);
  rotahost_falhou(e.url, 1); rotahost_falhou(e.url, 1);
  assert(rotahost_rota(e.url) == 0 && rotahost_modo(e.url) == 2);
  rotahost_falhou(e.url, 0);
  assert(rotahost_rota(e.url) == 0 && rotahost_modo(e.url) == 2);
  rotahost_falhou(e.url, 0);
  assert(rotahost_rota(e.url) == -1 && rotahost_modo(e.url) == -1);
  assert(strstr(disco, "\t0\t2\t2\t2\t"));
  rotahost_falhou("https://novo/live", 1); rotahost_falhou("https://novo/live", 1);
  assert(rotahost_rota("https://novo/live") == -1);
  recarregar(); assert(rotahost_rota(e.url) == -1 && rotahost_modo(e.url) == -1);
  rotahost_funcionou(e.url, 0, 2);
  assert(rotahost_plano(&e, p, 8) == 3); par(p, 0, 2, 0); par(p, 1, 2, 1); par(p, 2, 0, 0);
  rotahost_funcionou(e.url, 1, 0);
  assert(strstr(disco, "\t1\t0\t0\t0\t"));
  rotahost_falhou(e.url, 1);
  assert(rotahost_rota(e.url) == 1 && rotahost_modo(e.url) == 0);
  rotahost_falhou(e.url, 1);
  assert(rotahost_rota(e.url) == -1 && rotahost_modo(e.url) == -1);
  recarregar(); assert(rotahost_rota(e.url) == -1 && rotahost_modo(e.url) == -1);
  rotahost_funcionou(e.url, 1, 0);
  assert(rotahost_plano(&e, p, 8) == 3); par(p, 0, 0, 1); par(p, 1, 0, 0); par(p, 2, 2, 1);
  rotahost_limpar();
  assert(rotahost_plano(&e, p, 8) == 3); par(p, 0, 0, 1);
  e.proxyForcado = 0;
  assert(rotahost_plano(&e, p, 8) == 3); par(p, 0, 0, 0); par(p, 1, 0, 1); par(p, 2, 2, 0);
  e.modoForcado = 1;
  assert(rotahost_plano(&e, p, 8) == 2); par(p, 0, 1, 0); par(p, 1, 1, 1);
  e.proxyForcado = 1;
  assert(rotahost_plano(&e, p, 8) == 2); par(p, 0, 1, 1); par(p, 1, 1, 0);
  e.podeProxy = 0;
  assert(rotahost_plano(&e, p, 8) == 1); par(p, 0, 1, 0);
  e.modoForcado = e.proxyForcado = -1;
  assert(rotahost_plano(&e, p, 8) == 2); par(p, 1, 2, 0);
  e.variaModo = 0;
  assert(rotahost_plano(&e, p, 8) == 1);
  e.podeProxy = e.variaModo = 1;
  assert(rotahost_plano(&e, p, 1) == 1);
  assert(rotahost_plano(&e, p, 2) == 2);
  assert(!rotahost_plano(NULL, p, 3) && !rotahost_plano(&e, NULL, 3) && !rotahost_plano(&e, p, 0));
  rotahost_funcionou(NULL, 1, 0); rotahost_funcionou("", 0, 0);
  rotahost_funcionou(e.url, 2, 0); rotahost_falhou(e.url, -1);
  assert(rotahost_rota(e.url) == -1);
  // Datas distintas tornam a expulsao da entrada mais velha deterministica.
  char buf[16000] = ""; size_t n = 0;
  long long agora = (long long)time(NULL);
  for (int i = 0; i < ROTAHOST_MAX; i++)
    n += (size_t)snprintf(buf + n, sizeof buf - n, "host%d\t0\t0\t0\t0\t%lld\n", i, agora - 1000 + i);
  gravar("rotahost.txt", buf); recarregar();
  rotahost_funcionou("https://extra/live", 1, 2);
  assert(rotahost_rota("https://host0/live") == -1);
  assert(rotahost_rota("https://host1/live") == 0);
  assert(rotahost_rota("https://extra/live") == 1);
  snprintf(buf, sizeof buf, "velho\t1\t2\t0\t0\t%lld\nnovo\t0\t0\t0\t0\t%lld\n", agora - 7 * 86400 - 1, agora);
  gravar("rotahost.txt", buf); recarregar();
  assert(rotahost_rota("https://velho/path") == -1);
  assert(rotahost_modo("https://velho/path") == -1);
  assert(rotahost_rota("https://novo/path") == 0);
  gravar("rotahost.txt", "bad/path\t1\t0\t0\t0\t1\nmalformado\n"); recarregar();
  assert(rotahost_rota("https://bad/live") == -1);
  rotahost_limpar(); assert(!disco);
  puts("rotahost: tudo ok");
  return 0;
}
