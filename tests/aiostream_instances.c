// Exercise installed instances through the production transport and publication.
#include <stdlib.h>
#include <unistd.h>
#include "stub_perfisessao.h"
#include "../src/addons.c"
#include <assert.h>
int dados_gravar_leve(const char *name, const char *data) { (void)name; (void)data; return 1; }
static const char *resp[1];
char *rede_baixar(const char *url, int seconds) {
  (void)seconds;
  assert(strstr(url, "https://exemplo.test/config"));
  assert(strstr(url, "/stream/movie/tt1.json"));
  return strdup(resp[0]);
}
char *rede_baixar(const char *url, int seconds);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, seconds);
  if (status) *status = body ? 200 : 0;
  return body;
}
const char *rede_ultimo_erro(void) { return ""; }
const char *rede_url_publica(const char *url, char *dst, unsigned n) {
  (void)url; snprintf(dst, n, "fixture"); return dst;
}
Uint32 SDL_GetTicks(void) { return 1000; }
void fontecache_vod_limpar(void) {}
void sync_sujar_addons(const char *url) { (void)url; }
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
void stream_preparar_dica_p2p(const Stream *s, int n) { (void)s; (void)n; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
unsigned fontecache_vod_geracao(void) { return 0; }
const char *i18n(const char *s) { return s; }

int main(void) {
  static AddonRemoto remote[2];
  const char *manifest = "{\"id\":\"org.aiostreams\",\"name\":\"AIOStreams\","
                         "\"resources\":[\"stream\",\"catalog\"],\"catalogs\":[]}";
  for (int i = 0; i < 2; i++) {
    strcpy(remote[i].nome, "AIOStreams");
    snprintf(remote[i].url, sizeof remote[i].url,
             "https://exemplo.test/config%d/manifest.json", i);
    remote[i].ativo = 1;
  }
  assert(addons_definir_lista(remote, 2));
  for (int i = 0; i < 2; i++) addons_manifesto_lido(i, manifest);
  assert(addons_n() == 2);
  assert(!strcmp(addons_id_manifesto(0), addons_id_manifesto(1)));
  assert(!addons_definir_lista(remote, 2));
  assert(!addons_adicionar("AIOStreams", remote[0].url));
  resp[0] = "{\"streams\":[{\"url\":\"https://video.test/a.mp4\"}]}";
  Stream *streams = NULL;
  assert(addons_consultar("tt1", "movie", NULL, 2, NULL, NULL, &streams) == 2);
  assert(streams[0].addonOrdem == 0 && streams[1].addonOrdem == 1);
  assert(strcmp(streams[0].addonChave, streams[1].addonChave));
  free(streams);
  char a[192], b[192];
  addons_catalogo_chave("org.aiostreams", addons_base(0), "movie", "same", a, sizeof a);
  addons_catalogo_chave("org.aiostreams", addons_base(1), "movie", "same", b, sizeof b);
  assert(strcmp(a, b));
  desc_manifesto_cache_guardar(remote[0].url, 1, manifest);
  desc_manifesto_cache_guardar(remote[1].url, 1,
    "{\"id\":\"org.aiostreams\",\"name\":\"Second config\"}");
  char *cached0 = desc_manifesto_cache_obter(remote[0].url, 1);
  char *cached1 = desc_manifesto_cache_obter(remote[1].url, 1);
  assert(cached0 && cached1 && strcmp(cached0, cached1));
  free(cached0); free(cached1);
  assert(!strcmp(a, "org.aiostreams_movie_same")); // Existing row settings survive.
  assert(strstr(b, "~nv") == b);
  assert(addons_alternar(0) == 0);
  assert(addons_ativo(1));
  assert(addons_consultar("tt1", "movie", NULL, 2, NULL, NULL, &streams) == 1);
  assert(streams[0].addonOrdem == 1);
  free(streams);
  // Legacy saved lists remain readable, including two configs of one add-on.
  char dir[] = "/tmp/nuvio-aio-instances-XXXXXX", path[128];
  assert(mkdtemp(dir));
  snprintf(path, sizeof path, "%s/addons.txt", dir);
  FILE *file = fopen(path, "w"); assert(file);
  for (int i = 0; i < 2; i++) fprintf(file, "AIOStreams\t%s\n", remote[i].url);
  fclose(file);
  assert(addons_carregar(dir) == 2);
  unlink(path); rmdir(dir);
  assert(addons_consultar("tt1", "movie", NULL, 2, NULL, NULL, &streams) == 2);
  free(streams);
  puts("AIOStreams instances: same manifest ID, separate streams and catalog keys PASS");
  return 0;
}
