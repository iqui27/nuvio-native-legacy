// Run the actual TV pairing thread against a fake pairing service and instance.
#define NV_REC_URL "https://pair.invalid"
#include "../src/aiopair.c"
#include <assert.h>
static int mode, takes, instanceCalls, results;
static char sentResult[32];
int rede_pedir(const RedePedido *q,RedeResposta *r) {
  memset(r,0,sizeof *r); r->status=200; r->erro=REDE_OK;
  assert(!q->seguir && !q->tls_pessoal);
  assert(!strchr(q->url,'?') && !strstr(q->url,"synthetic-password"));
  if(strstr(q->url,"/v1/aiopair/")) {
    char op[24]; js_texto_raiz(q->corpo,"op",op,sizeof op);
    assert(!strstr(q->corpo,"synthetic-password"));
    if(!strcmp(op,"take")) {
      if(takes++) { r->status=410; return 0; }
      r->corpo=strdup(mode==2?
        "{\"origin\":\"https://other.invalid\",\"uuid\":\"sample-uuid\",\"password\":\"synthetic-password\"}":
        "{\"origin\":\"https://instance.invalid\",\"uuid\":\"sample-uuid\",\"password\":\"synthetic-password\"}");
    } else {
      if(!strcmp(op,"result")) { results++; js_texto_raiz(q->corpo,"state",sentResult,sizeof sentResult); }
      r->corpo=strdup("{\"state\":\"waiting\"}");
    }
  } else {
    instanceCalls++; assert(!strcmp(q->url,"https://instance.invalid/api/v1/user"));
    assert(q->cabecalhos && !strncmp(q->cabecalhos[0],"Authorization: Basic ",21));
    if(mode==1) r->status=401;
    else if(mode==3) r->status=302;
    r->corpo=strdup("{\"success\":true,\"data\":{\"userData\":{\"presets\":[{\"type\":\"Torrentio\"}]}}}");
  }
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); memset(r,0,sizeof *r); }
int main(void) {
  for(mode=0;mode<4;mode++) {
    aiopair_close(); takes=instanceCalls=results=0;
    Pair *p=calloc(1,sizeof *p); p->generation=generation;
    strcpy(p->origin,"https://instance.invalid");
    assert(randomKey(p->id) && randomKey(p->tv) && randomKey(p->phone));
    assert(strcmp(p->tv,p->phone) && strcmp(p->id,p->phone));
    run(p);
    char auth[2048],origin[256];
    assert(aiopair_take(auth,sizeof auth,origin,sizeof origin)==(mode==0));
    if(!mode) { assert(!strcmp(origin,"https://instance.invalid")); assert(!strcmp(sentResult,"synced")); }
    else assert(!strcmp(sentResult,"error"));
    assert(results==1 && instanceCalls==(mode==2?0:1));
    assert(!aiopair_take(auth,sizeof auth,origin,sizeof origin));
    memset(auth,0,sizeof auth);
  }
  aiopair_close(); assert(state==AIOPAIR_OFF && !savedAuth[0] && !savedOrigin[0] && !pairUrl[0]);
  assert(!aiopair_start("http://instance.invalid"));
  puts("aiopair TV: valid/wrong credentials, origin mismatch, redirects, one-time consumption and cleanup passed");
}
