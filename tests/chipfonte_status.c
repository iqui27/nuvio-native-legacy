// Exercise actual verification publishers without network or a TV.
#define rede_url_final_tipo chipProbe
#include "../src/streams.c"
#include <assert.h>
static int succeeds=1, replaceList, sawChecking;
int chipProbe(const char *url, int seconds, const char *const *headers,
              char *dst, unsigned len, int *status, char *mime, unsigned mimeLen, long *bytes) {
  (void)seconds; (void)headers;
  sawChecking=stream_fonte_estado(0)==FA_CONFERINDO;
  if (replaceList) stream_definir_lista(NULL,0);
  snprintf(dst,len,"%s",url); snprintf(mime,mimeLen,"video/mp4");
  *status=200; *bytes=1000000;
  return succeeds;
}
int main(void) {
  static Stream s={0};
  snprintf(s.url,sizeof s.url,"https://example.invalid/source.mp4"); s.altura=1080;
  stream_definir_alvo("tt-chipfonte"); stream_definir_lista(&s,1);
  assert(stream_fonte_estado(0)==FA_NADA);
  Conferencia c={.geracao=stream_lista_geracao(),.sessao=nv_perfil_sessao_ler(),.antecipada=-1};
  fontefalha_filtro(&c.filtro);
  assert(verificarOuParar(0,&c));
  assert(sawChecking && stream_fonte_estado(0)==FA_OK);
  succeeds=0; assert(!verificarOuParar(0,&c));
  assert(sawChecking && stream_fonte_estado(0)==FA_RUIM);
  s.emCache=1; stream_definir_lista(&s,1); assert(stream_fonte_estado(0)==FA_OK);
  s.cacheNegado=1; stream_definir_lista(&s,1); assert(stream_fonte_estado(0)==FA_NADA);
  s.conferencia=FA_RUIM; s.cacheNegado=0;
  stream_definir_lista(&s,1); assert(stream_fonte_estado(0)==FA_RUIM);
  s.conferencia=FA_NADA; s.emCache=0; s.tamanhoBytes=UINT64_C(19756849562);
  stream_definir_lista(&s,1);
  assert(tamanhoMBFonte(stream_item(0))==18841);
  succeeds=1; replaceList=1; c.geracao=stream_lista_geracao();
  assert(!verificarOuParar(0,&c)); assert(stream_fonte_estado(0)==FA_NADA);
  puts("PASS: real probes publish checking/ready/failed; unknown cache stays unknown; stale list ignored");
  return 0;
}
