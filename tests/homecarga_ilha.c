// QUEM LIGA O ALERTA "CARREGAMENTO DA HOME" DA ILHA (dono, 03/10: "tem alguma
// coisa sempre dando trigger no alerta da ilha do relogio na home loading").
//
// app.c abre o alerta enquanto desc_home_carga().ativo e 1. Ate aqui `ativo`
// era desc_montando(): a volta de catalogos OU o refazer do "Continuar
// assistindo" (cwVivo/cwDeNovo) OU uma volta pedida pelo sync. Esses tres
// disparam em segundo plano — a cada 10 min na home, a cada saida do player,
// a cada sync — e a ilha anunciava "Carregando fileiras…" sem a pessoa ter
// pedido nada. Aqui: so a volta VISIVEL (arranque, ou pedido da pessoa) acende.
//
// Reaproveita as dubles de montagem_cedo.c (descoberta.c inteiro, rede falsa).
#define main cedo_main
#include "montagem_cedo.c"
#include "jellyfin_stub.inc"
#include "nvtmp.h"
// descoberta.c now records the manifest and the watch-state destination; this
// fixture links neither addons.c nor watchstate.c.
__attribute__((weak)) void addons_manifesto_lido_base(int i, const char *b, const char *d, unsigned v, const char *c) {
  (void)i; (void)b; (void)d; (void)v; (void)c; }
__attribute__((weak)) void ws_destino(const char *b, const char *u, int p, char out[20]) {
  (void)b; (void)u; (void)p; out[0] = 0; }
#undef main


static volatile unsigned segurarMs;
static void segurar(const char *id) { (void)id; if (segurarMs) usleep(segurarMs * 1000); }

static int ativo(void) { DescHomeCarga c; desc_home_carga(&c); return c.ativo; }
static void esperarFim(void) { esperarQuieto(); }

int main(void) {
  const char *tmp = getenv("TMPDIR");
  snprintf(dirDados, sizeof dirDados, "%snuvio-homecarga-%d", nvt("/"), (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);
  homeestado_iniciar();
  fixtureCatalogo = segurar;

  // 1. arranque: visivel, e apaga no fim.
  segurarMs = 150;
  desc_iniciar();
  usleep(80 * 1000);
  assert(desc_montando() && ativo());
  esperarFim();
  assert(!desc_montando() && !ativo());
  puts("ok  volta do arranque acende o alerta e apaga no fim");

  // 2. "Continuar assistindo" refeito em segundo plano (10 min na home, saida
  // do player, sync): desc_montando() fica 1, o alerta NAO. A bandeira e a
  // mesma que fioContinuar liga/desliga.
  cwVivo = 1;
  assert(desc_montando());
  printf("    refazer Continuar: desc_montando=%d alerta=%d\n", desc_montando(), ativo());
  assert(!ativo());
  cwVivo = 0; cwDeNovo = 1;
  assert(!ativo());
  cwDeNovo = 0;
  puts("ok  refazer o Continuar assistindo nao acende o alerta");

#ifndef ANTES
  // 3. volta pedida em segundo plano (sync): roda e NAO acende.
  desc_repetir_silencioso();
  usleep(80 * 1000);
  assert(desc_montando() && !ativo());
  esperarFim();
  assert(!ativo());
  puts("ok  volta silenciosa (sync) nao acende o alerta");

  // 4. pedido da pessoa (idioma, addons, ordem): acende.
  desc_repetir();
  usleep(80 * 1000);
  assert(ativo());
  esperarFim();
  assert(!ativo());
  puts("ok  volta pedida pela pessoa acende o alerta");

  // 5. silenciosa em andamento + pedido da pessoa: passa a visivel; e o
  // contrario (pedido silencioso sobre uma visivel) nao a esconde.
  desc_repetir_silencioso();
  usleep(80 * 1000);
  assert(!ativo());
  desc_repetir();
  assert(ativo());
  desc_repetir_silencioso();
  assert(ativo());
  esperarFim();
  assert(!ativo());
  puts("ok  silenciosa promovida por pedido da pessoa; silenciosa nao esconde visivel");
#endif
  puts("\nTODOS OS CASOS PASSARAM");
  return 0;
}
