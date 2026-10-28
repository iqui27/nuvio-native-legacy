// Exercise the production AIO caller against a local HTTP/TLS fixture.
#include "stub_perfisessao.h"
void addonstats_salvar(void);
#include "../src/addons.c"
#include <assert.h>
int main(int argc,char **argv) {
  assert(argc==3);
  AddonPedido origin={.aio=1}; strcpy(origin.nome,"AIOStreams");
  char url[4096];
  const char *routes[]={"valid","empty","mixed","401","429","503","invalid"};
  AddonAioRequest *record=calloc(1,sizeof *record); assert(record);
  for(int i=0;i<7;i++) {
    snprintf(origin.estatistica,sizeof origin.estatistica,"fixture-%d",i);
    snprintf(url,sizeof url,"%s/proxy/stremio/alias/variant/%s/stream/series/tt1:1:2.json?variant=fixture%%2Fone&mode=full",argv[1],routes[i]);
    RedeMedida m={0}; int missing=-1;
    char *body=baixarStream(&origin,"series",url,3,&missing,&m);
    if(i<3) assert(body && missing==0 && m.status==200);
    else assert(!body);
    free(body);
    assert(addonstats_aio_history(origin.estatistica,"series",record,1)==1);
    if(i==0) assert(record->state==AIO_REQUEST_OK && record->playable==1 && record->statistics==-1);
    if(i==1) assert(record->state==AIO_REQUEST_EMPTY && record->playable==0 && record->errors==-1);
    if(i==2) assert(record->state==AIO_REQUEST_SOURCE_ERROR && record->playable==1 && record->errors==1 && record->evidence[0].timeout==1);
    if(i==3) assert(record->state==AIO_REQUEST_CONFIG_LOCKED);
    if(i==4) {
      assert(record->state==AIO_REQUEST_RATE_LIMITED && addonstats_aio_wait(origin.estatistica,"series")>0);
      assert(!baixarStream(&origin,"series",url,3,NULL,NULL));
      assert(addonstats_aio_history(origin.estatistica,"series",record,10)==1);
    }
    if(i==5) assert(record->state==AIO_REQUEST_HTTP_ERROR);
    if(i==6) assert(record->state==AIO_REQUEST_INVALID);
  }
  strcpy(origin.estatistica,"tls-failure");
  snprintf(url,sizeof url,"%s/valid",argv[2]);
  assert(!baixarStream(&origin,"series",url,3,NULL,NULL));
  assert(addonstats_aio_history(origin.estatistica,"series",record,1)==1);
  assert(record->state==AIO_REQUEST_TRANSPORT_ERROR && record->playable==-1);
  free(record); addonstats_aio_reset();
  puts("AIO local transport: identity, exact variant, mixed notices, HTTP failures, TLS and Retry-After PASS");
}
