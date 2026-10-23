// Boundary for isolated app.c fixtures. Full-source tests link src/m3u.c;
// tests/m3u.sh exercises that real importer, cache and profile ownership.
#ifndef NV_TEST_STUB_M3U_H
#define NV_TEST_STUB_M3U_H
#include <assert.h>
static char m3uTesteUrl[4096], m3uTesteCab[512];
int m3u_e_id(const char *id) { return id && !strncmp(id,"nvm3u:",6) && strlen(id)==22; }
int m3u_resolver_sessao(const char *id,char *url,unsigned n,uint64_t sessao,int renovar) {
  (void)sessao;(void)renovar;assert(m3u_e_id(id)&&m3uTesteUrl[0]);
  int k=snprintf(url,n,"%s",m3uTesteUrl);return k>0&&(unsigned)k<n;
}
int m3u_cabecalhos(const char *id,char *out,unsigned n) {
  assert(m3u_e_id(id));int k=snprintf(out,n,"%s",m3uTesteCab);return k>=0&&(unsigned)k<n;
}
#endif
