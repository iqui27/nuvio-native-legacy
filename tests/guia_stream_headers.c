// Caminho real de app.c: cada fonte do guia deve armar os headers do video.
#define NV_PERFIL_SESSAO_FORTE
#define video_definir_cabecalhos teste_video_cabecalhos
#define proxyts_definir_cabecalhos teste_proxy_cabecalhos
#define proxyts_disponivel teste_proxy_disponivel
#define player_id_canal teste_id_canal
#define ajustes_livetv_proxy teste_proxy_ativo
#include "../src/app.c"
#include "stub_m3u.h"
#include <assert.h>

uint64_t perfis_sessao_geracao(void){return 0;}
int perfis_sessao_atual(uint64_t s){return s==0;}
int perfis_sessao_travar(uint64_t s){return s==0;}
void perfis_sessao_destravar(void){}
int stalker_resolver_sessao(const char *id,char *u,unsigned n,uint64_t s){(void)id;(void)u;(void)n;(void)s;assert(!"M3U must not use the Stalker resolver");return 0;}
static char videoCabs[512], proxyCabs[512];
static int proxyAtivo;
static Stream publicada;
void stream_definir_lista(const Stream *s,int n){assert(n==1);publicada=*s;}
void teste_video_cabecalhos(const char *c) { snprintf(videoCabs, sizeof videoCabs, "%s", c ? c : ""); }
void teste_proxy_cabecalhos(const char *c) { snprintf(proxyCabs, sizeof proxyCabs, "%s", c ? c : ""); }
int teste_proxy_disponivel(void) { return 1; }
int teste_proxy_ativo(void) { return proxyAtivo; }
int guia_proxy_do_canal(const char *id, int padrao) { (void)id; return padrao; }
const char *teste_id_canal(void) { return "m3u:canaisabertos:0"; }
int xtream_e_id(const char *id) { (void)id; return 0; }
int stalker_e_id(const char *id) { (void)id; return 0; }
int xtream_url_limitada(const char *u) { (void)u; return 0; }
int xtream_conexoes_limitadas(void) { return 0; }

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
  // A confirmed continuous TS uses the same direct URL as a manual pick.
  proxyAtivo = 1;
  s[0].tsContinuo = 1;
  assert(!strcmp(urlCanal(&s[0], url, sizeof url), s[0].url));
  assert(!strcmp(videoCabs, s[0].cabecalhos));
  // Native M3U uses its own resolver worker, then arms the same video headers.
  FonteJob *job=calloc(1,sizeof *job);assert(job);
  strcpy(job->id,"nvm3u:0123456789abcdef");job->sessaoPerfil=nv_perfil_sessao_ler();job->renovando=1;
  strcpy(m3uTesteUrl,"https://cdn.example/native.m3u8");strcpy(m3uTesteCab,"Referer: https://required.example/\n");
  escolherFonteStalker(job);
  assert(atomic_load(&job->estado)==FJOB_DONE&&job->resultado==0&&!strcmp(job->url,m3uTesteUrl));
  assert(montarCanalStalker(job->id,job->url)==0&&!strcmp(publicada.provedor,"m3u"));
  assert(!strcmp(publicada.cabecalhos,m3uTesteCab));
  proxyAtivo=0;urlCanal(&publicada,url,sizeof url);assert(!strcmp(videoCabs,m3uTesteCab));free(job);
  free(s);
  puts("PASS: guia -> stream_parse -> headers de cada pedido de video/proxy");
  return 0;
}
