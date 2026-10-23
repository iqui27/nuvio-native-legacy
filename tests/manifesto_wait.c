// Exercise the actual manifest consumer and worker, without network access.
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_filmes_vistos(void) { return 0; }    // #366: descoberta.c le o ajuste
#include "../src/descoberta.c"
#include <assert.h>
#include "stub_perfisessao.h"
int addons_identidade_recebida(const char *base, unsigned version, uint64_t session, const char *body) {
  (void)base; (void)version; (void)session; (void)body; return 1;
}
#include <stdatomic.h>
#include <unistd.h>
// maniObter polls with SDL_Delay (3ab30f46); this test links no SDL, so sleep for real.
void SDL_Delay(Uint32 ms) { usleep(ms * 1000u); }
static _Atomic int requests;
static int fixtureAddons;
char *rede_baixar(const char *url, int seconds) {
  (void)url; (void)seconds; requests++; return NULL;
}
void addons_lista_travar(void) {}
void addons_lista_destravar(void) {}
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
void ws_destino(const char *b, const char *u, int p, char out[20]) {
  (void)b; (void)u; (void)p; strcpy(out, "fixture");
}
unsigned addons_versao(void) { return 1; }
int addons_n(void) { return fixtureAddons; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
int addons_ativo(int i) { return i != 0; }
const char *addons_base(int i) {
  static const char *urls[] = {"https://disabled.invalid", "https://active1.invalid", "https://active2.invalid"};
  return urls[i];
}
int main(void) {
  const char *url = "https://fixture.invalid/manifest.json";
  maniN = 1; maniGeracao = 1;
  maniVersao = addons_versao(); maniSessao = descSessao();
  snprintf(mani[0].url, sizeof mani[0].url, "%s", url);
  mani[0].pronto = 0;
  assert(maniObter(url, 0) == NULL && requests == 0); // disabled, still pending
  mani[0].pronto = 1;
  assert(maniObter(url, 1) == NULL && requests == 0); // failed, no second timeout
  mani[0].corpo = strdup("{\"id\":\"ok\"}");
  char *body = maniObter(url, 1);
  assert(body && !strcmp(body, "{\"id\":\"ok\"}")); free(body);
  assert(maniObter("https://missing.invalid/manifest.json", 0) == NULL && requests == 0);
  assert(maniObter("https://missing.invalid/manifest.json", 1) == NULL && requests == 1);
  // Cached first slot is skipped by worker, not fetched for a second time.
  maniN = 2; maniProx = 0; mani[0].pronto = 1; mani[1].pronto = 0;
  snprintf(mani[1].url, sizeof mani[1].url, "%s", url);
  fioManifesto((void *)(uintptr_t)1);
  assert(requests == 2 && mani[1].pronto && maniProx == 2);
  // All-cache startup still serves cached bodies without spawning workers.
  fixtureAddons = 3;
  for (int i = 0; i < fixtureAddons; i++) {
    char u[100]; snprintf(u, sizeof u, "%s/manifest.json", addons_base(i));
    desc_manifesto_cache_guardar(u, 1, "{\"id\":\"cached\"}");
  }
  int before = requests;
  maniLargar();
  assert(maniN == 3 && mani[0].ativo && mani[1].ativo && !mani[2].ativo);
  body = maniObter("https://active1.invalid/manifest.json", 1);
  assert(body && requests == before); free(body);
  puts("PASS: disabled manifests do not block, failed manifests are not retried, cache slots are skipped");
  return 0;
}
