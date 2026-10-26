// Compare the production foreground and captured workers without network access.
#define main addonslista_fixture_main
#define rede_baixar fixture_legacy_download
#define rede_baixar_st fixture_legacy_download_st
#include "addonslista.c"
#undef main
#undef rede_baixar
#undef rede_baixar_st
char *rede_baixar(const char *, int);
char *rede_baixar_st(const char *, int, const char *const *, int *);
#include "../src/addons.c"
#include <assert.h>

void debrid_contexto_capturar(DebridContexto *c) { memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
static char requests[4096];
char *rede_baixar(const char *url, int seconds) {
  if (strstr(url, "arm.haglund.dev")) return fixture_legacy_download(url, seconds);
  assert(strlen(requests) + strlen(url) + 2 < sizeof requests);
  strcat(requests, url); strcat(requests, "\n");
  return strdup("{\"streams\":[{\"url\":\"https://fixture.invalid/video.mp4\"}]}");
}
char *rede_baixar_st(const char *url, int seconds, const char *const *h, int *status) {
  (void)h; *status = 200; return rede_baixar(url, seconds);
}
int main(void) {
  nAddon = 2; memset(addon, 0, sizeof addon);
  for (int i = 0; i < nAddon; i++) {
    addon[i].ativo = addon[i].fonte = addon[i].sondado = 1;
    snprintf(addon[i].nome, sizeof addon[i].nome, "Provider %d", i);
    snprintf(addon[i].base, sizeof addon[i].base, "https://provider%d.invalid/config", i);
    strcpy(addon[i].streamTipos, i ? "|movie|" : "|series|");
    strcpy(addon[i].streamPref[0], "tt"); addon[i].nStreamPref = 1;
  }
  // The series metadata uses Kitsu; the stream addon accepts IMDb only.
  const char *id = "kitsu:7442:5";
  Stream *out = NULL;
  addons_definir_origem(addon[0].base);
  addons_buscar(id, "series");
  assert(fioVivo); pthread_join(fio, NULL); fioVivo = 0;
  assert(nResultado == 1);
  char detail[sizeof requests]; strcpy(detail, requests);
  assert(strstr(detail, "/stream/series/tt2560140:1:5.json"));
  assert(!strstr(detail, "provider1"));
  free(resultado); resultado = NULL; nResultado = 0;
  AddonsConsulta *snapshot = addons_consulta_capturar(); assert(snapshot);
  // Both next-episode preload and saved-source renewal use this entry point.
  for (int path = 0; path < 2; path++) {
    requests[0] = 0;
    int count = addons_consulta_consultar(snapshot, id, "series", 1, NULL, NULL, &out);
    assert(!strcmp(detail, requests)); // identical type, episode ID and configured addon URL
    assert(count == 1 && out);
    char key[FONTEFALHA_CHAVE]; fontefalha_addon_chave(addon[0].base, key, sizeof key);
    assert(!strcmp(out[0].addonChave, key));
    free(out); out = NULL;
  }
  addons_consulta_liberar(snapshot);
  puts("PASS Detail, next episode and same-addon renewal request parity");
}
