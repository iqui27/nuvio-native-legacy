#include "importarfluxo.h"
#include "importar.h"
#include "celbotao.h"
#include "js.h"
#include "rede.h"
#include "salvos.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STREMIO "https://api.strem.io/api/"
#define TITULOS_MAX 300
#define ESPERA_S 300

static volatile ImpfEstado estado = IMPF_PARADO;
static volatile int fonte, geracao, pronto;
static char codigo[24], link[200], host[64];
static int nAddons, nTitulos;
static AddonRemoto addons[ADDONS_MAX];
static ImpTitulo titulos[TITULOS_MAX];
static char corpoAiom[NV_ADDON_URL_MAX];
static const char *const JSON[] = { "Content-Type: application/json", "Accept: application/json", NULL };

static int vivo(int g) { return g == geracao; }

static char *stremio(const char *rota, const char *corpo, int *st) {
  char u[96];
  snprintf(u, sizeof u, STREMIO "%s", rota);
  return rede_postar_st(u, 20, JSON, corpo, st);
}

static void *stremio_fio(void *arg) {
  int g = (int)(intptr_t)arg, st = 0;
  char *r = stremio("linkCreate", "{}", &st);
  static char chave[200];
  chave[0] = 0;
  if (r && vivo(g)) {
    if (js_texto(r, NULL, "code", codigo, sizeof codigo) && js_texto(r, NULL, "link", link, sizeof link))
      estado = IMPF_ESPERA;
  }
  free(r);
  for (time_t fim = time(NULL) + ESPERA_S; vivo(g) && estado == IMPF_ESPERA && time(NULL) < fim;) {
    char u[128];
    struct timespec dorme = { 2, 0 };
    nanosleep(&dorme, NULL);
    if (!vivo(g)) return NULL;
    snprintf(u, sizeof u, STREMIO "linkReadData?code=%s", codigo);
    r = rede_baixar_st(u, 15, JSON, &st);
    if (r && js_texto(r, NULL, "authKey", chave, sizeof chave)) { free(r); break; }
    free(r);
  }
  if (!vivo(g)) return NULL;
  if (!chave[0]) { estado = IMPF_ERRO; return NULL; }
  estado = IMPF_TRABALHA;
  {
    static char corpo[320];
    snprintf(corpo, sizeof corpo, "{\"type\":\"AddonCollectionGet\",\"authKey\":\"%s\",\"update\":true}", chave);
    r = stremio("addonCollectionGet", corpo, &st);
    nAddons = r ? importar_stremio_addons(r, addons, ADDONS_MAX) : 0;
    free(r);
    snprintf(corpo, sizeof corpo, "{\"authKey\":\"%s\",\"collection\":\"libraryItem\",\"ids\":[],\"all\":true}", chave);
    r = stremio("datastoreGet", corpo, &st);
    nTitulos = r ? importar_stremio_biblioteca(r, titulos, TITULOS_MAX) : 0;
    free(r);
    memset(corpo, 0, sizeof corpo);
    memset(chave, 0, sizeof chave);
  }
  if (!vivo(g)) return NULL;
  if (!nAddons && !nTitulos) { estado = IMPF_ERRO; return NULL; }
  pronto = 1;
  return NULL;
}

static void *aiom_fio(void *arg) {
  int g = (int)(intptr_t)arg;
  char *r = rede_baixar(corpoAiom, 20);
  memset(corpoAiom, 0, sizeof corpoAiom);
  if (!vivo(g)) { free(r); return NULL; }
  nAddons = r ? importar_aiom_addons(r, addons, ADDONS_MAX) : 0;
  free(r);
  if (!nAddons) { estado = IMPF_ERRO; return NULL; }
  pronto = 1;
  return NULL;
}

static void lancar(void *(*f)(void *)) {
  pthread_t t;
  if (pthread_create(&t, NULL, f, (void *)(intptr_t)geracao)) { estado = IMPF_ERRO; return; }
  pthread_detach(t);
}

void importarfluxo_cancelar(void) {
  geracao++;
  pronto = 0;
  estado = IMPF_PARADO;
  celb_fechar_dono(CELB_IMPORTAR);
}

void importarfluxo_comecar(int f) {
  importarfluxo_cancelar();
  fonte = f;
  codigo[0] = link[0] = host[0] = 0;
  nAddons = nTitulos = 0;
  if (f == IMPF_STREMIO) { estado = IMPF_TRABALHA; lancar(stremio_fio); }
  else if (f == IMPF_AIOM) {
    estado = celb_disponivel() && celb_abrir_embutido(CELB_IMPORTAR, "Link do AIOManager") ? IMPF_ESPERA : IMPF_ERRO;
  }
}

void importarfluxo_passo(void) {
  if (fonte == IMPF_AIOM && estado == IMPF_ESPERA && celb_pegar(CELB_IMPORTAR, corpoAiom, sizeof corpoAiom)) {
    importar_host(corpoAiom, host, sizeof host);
    if (!host[0]) { memset(corpoAiom, 0, sizeof corpoAiom); estado = IMPF_ERRO; return; }
    estado = IMPF_TRABALHA;
    lancar(aiom_fio);
  }
  if (!pronto) return;
  pronto = 0;
  for (int i = 0; i < nAddons; i++) addons_adicionar(addons[i].nome, addons[i].url);
  for (int i = 0; i < nTitulos; i++) {
    CatItem ci = {0};
    snprintf(ci.imdb, sizeof ci.imdb, "%s", titulos[i].imdb);
    snprintf(ci.tipo, sizeof ci.tipo, "%s", !strcmp(titulos[i].tipo, "series") ? "series" : "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "%s", titulos[i].nome);
    salvos_definir(&ci, 1);
  }
  memset(addons, 0, sizeof addons);
  celb_fechar_dono(CELB_IMPORTAR);
  estado = IMPF_OK;
}

ImpfEstado  importarfluxo_estado(void) { return estado; }
int         importarfluxo_fonte(void) { return fonte; }
const char *importarfluxo_codigo(void) { return codigo; }
const char *importarfluxo_link(void) { return link; }
const char *importarfluxo_host(void) { return host; }
int         importarfluxo_addons(void) { return nAddons; }
int         importarfluxo_titulos(void) { return nTitulos; }

#ifdef AJUSTES_TESTE
// Screenshot hook: forces a state without network.
void importarfluxo_teste(int f, ImpfEstado e, const char *cod, const char *lnk, int na, int nt) {
  importarfluxo_cancelar();
  fonte = f; estado = e; nAddons = na; nTitulos = nt;
  snprintf(codigo, sizeof codigo, "%s", cod);
  snprintf(link, sizeof link, "%s", lnk);
}
#endif
