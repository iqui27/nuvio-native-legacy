// Real HTTPS transport for the phone/worker/TV integration fixture.
static const char *testService, *testCa;
#define NV_REC_URL testService
#define rede_pedir testTransport
#include "../src/aiopair.c"
#undef rede_pedir
#include <assert.h>
int rede_pedir(const RedePedido *,RedeResposta *);
int testTransport(const RedePedido *q,RedeResposta *r) {
  RedePedido trusted=*q; trusted.ca_arquivo=testCa;
  return rede_pedir(&trusted,r);
}
int main(int argc,char **argv) {
  assert(argc==6); testService=argv[1];testCa=argv[3];
  assert(aiopair_start(argv[2]));
  char url[512];int s=0;
  for(int i=0;i<100;i++) { s=aiopair_state(url,sizeof url);if(s==AIOPAIR_WAITING)break;usleep(100000); }
  assert(s==AIOPAIR_WAITING);
  FILE *f=fopen(argv[4],"w");assert(f);fputs(url,f);fclose(f);
  for(int i=0;i<180;i++) {
    s=aiopair_state(NULL,0);
    if(s==AIOPAIR_SYNCED)break;
    usleep(100000);
  }
  char auth[2048],origin[256];
  assert(s==AIOPAIR_SYNCED && aiopair_take(auth,sizeof auth,origin,sizeof origin));
  assert(!strcmp(origin,argv[2]));memset(auth,0,sizeof auth);aiopair_close();
  f=fopen(argv[5],"w");assert(f);fputs("ok",f);fclose(f);
  return 0;
}
