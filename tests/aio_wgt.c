// Compile the production caller for WGT; emulate its two transport contracts.
#include "stub_perfisessao.h"
void addonstats_salvar(void);
#include "../src/addons.c"
#include <assert.h>

static const char *payload;
static int httpStatus, requests;
static const char *variant = "https://fixture.invalid/config/stream/series/tt1:1:2.json?variant=one%2Ftwo&mode=full";
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p;
  memset(r, 0, sizeof *r);
  r->erro = REDE_INDISPONIVEL; // WGT rejects buffered requests with headers/redirects.
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); }
char *rede_baixar_medido(const char *url, int seconds, const char *const *headers, RedeMedida *m) {
  assert(!strcmp(url, variant) && seconds == 3);
  assert(headers && !strcmp(headers[0], "User-Agent: AIO-NuvioLegacy") && !headers[1]);
  requests++;
  *m = (RedeMedida){.status=httpStatus, .bytes=payload ? (long)strlen(payload) : 0, .ms=17};
  return payload ? strdup(payload) : NULL;
}
int main(void) {
  AddonPedido origin = {.aio=1};
  AddonAioRequest *record = calloc(1, sizeof *record); assert(record);
  const char *bodies[] = {"{\"streams\":[{\"url\":\"https://video.invalid/a.mp4\"}]}", "{\"streams\":[]}", "broken", "{}", NULL};
  for (int i=0; i<5; i++) {
    snprintf(origin.estatistica, sizeof origin.estatistica, "wgt-%d", i);
    payload=bodies[i]; httpStatus=i==3 ? 401 : i==4 ? 0 : 200;
    int missing=-1; RedeMedida m={0};
    char *body=baixarStream(&origin, "series", variant, 3, &missing, &m);
    assert(requests==i+1);
    assert(m.status==httpStatus && m.ms==17);
    if(i<2) assert(body && missing==0); else assert(!body);
    free(body);
    assert(addonstats_aio_history(origin.estatistica, "series", record, 1)==1);
    const int states[]={AIO_REQUEST_OK, AIO_REQUEST_EMPTY, AIO_REQUEST_INVALID, AIO_REQUEST_CONFIG_LOCKED, AIO_REQUEST_TRANSPORT_ERROR};
    assert(record->state==states[i]);
  }
  free(record); addonstats_aio_reset();
  puts("AIO WGT caller: identity, exact variant, streams, validation and failure history PASS");
}
