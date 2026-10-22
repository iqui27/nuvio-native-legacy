// Caminho real de app.c: cada fonte do guia deve armar os headers do video.
#define video_definir_cabecalhos teste_video_cabecalhos
#define proxyts_definir_cabecalhos teste_proxy_cabecalhos
#define proxyts_disponivel teste_proxy_disponivel
#define player_id_canal teste_id_canal
#define ajustes_livetv_proxy teste_proxy_ativo
#include "../src/app.c"
#include <assert.h>

static char videoCabs[512], proxyCabs[512];
static int proxyAtivo;
void teste_video_cabecalhos(const char *c) { snprintf(videoCabs, sizeof videoCabs, "%s", c ? c : ""); }
void teste_proxy_cabecalhos(const char *c) { snprintf(proxyCabs, sizeof proxyCabs, "%s", c ? c : ""); }
int teste_proxy_disponivel(void) { return 1; }
int teste_proxy_ativo(void) { return proxyAtivo; }
const char *teste_id_canal(void) { return "m3u:canaisabertos:0"; }
int xtream_e_id(const char *id) { (void)id; return 0; }
int stalker_e_id(const char *id) { (void)id; return 0; }

int main(void) {
  Stream *s = NULL;
  char url[4200];
  const char *json = "{\"streams\":[{\"url\":\"https://cdn.example/bandsp/index.m3u8\","
    "\"behaviorHints\":{\"notWebReady\":true,\"proxyHeaders\":{\"request\":{"
    "\"User-Agent\":\"Teste/1\",\"Referer\":\"https://cdnembedcanais.xyz/\","
    "\"Origin\":\"https://cdnembedcanais.xyz\"}}}},"
    "{\"url\":\"https://cdn.example/segunda.m3u8\",\"behaviorHints\":{"
    "\"proxyHeaders\":{\"request\":{\"Referer\":\"https://outro.example/\"}}}},"
    "{\"url\":\"https://cdn.example/sem.m3u8\"}]}";
  assert(stream_extrair(json, "Bsweb TV", &s) == 3);
  for (proxyAtivo = 0; proxyAtivo <= 1; proxyAtivo++) {
    for (int k = 0; k < 3; k++) {
      const char *u = urlCanal(&s[k], url, sizeof url);
      assert(!strcmp(videoCabs, s[k].cabecalhos));
      if (proxyAtivo) {
        assert(!strncmp(u, PROXYTS_PREFIXO, strlen(PROXYTS_PREFIXO)));
        assert(!strcmp(proxyCabs, s[k].cabecalhos));
      } else assert(!strcmp(u, s[k].url));
      if (k == 0) {
        assert(strstr(videoCabs, "Referer: https://cdnembedcanais.xyz/"));
        assert(strstr(videoCabs, "Origin: https://cdnembedcanais.xyz"));
        assert(strstr(videoCabs, "User-Agent: Teste/1"));
      }
    }
  }
  free(s);
  puts("PASS: guia -> stream_parse -> headers de cada pedido de video/proxy");
  return 0;
}
