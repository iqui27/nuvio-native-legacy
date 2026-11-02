// Exercise the shipped TV thread against offline worker/instance fixtures.
static const char *fixtureService="https://pair.invalid";
#define NV_REC_URL fixtureService
#include <unistd.h>
static int testSleep(unsigned usec) { (void)usec; return 0; }
#define usleep testSleep
#include "../src/aiopair.c"
#include <assert.h>
enum { GOOD, BAD_AUTH, WRONG_ORIGIN, REDIRECT, EXPIRED, POLL_NETWORK,
       POLL_SERVER, CREATE_HTML, CONFIRM_SERVER, CANCEL_CREATE, CANCEL_VALIDATE,
       INSTANCE_NETWORK, INSTANCE_SERVER, BAD_RESPONSE, RETRY_CONFIRM, RETRY_TAKE, CASES };
static int mode, takes, instanceCalls, results, closes;
static char sentResult[32];
int rede_pedir(const RedePedido *q,RedeResposta *r) {
  memset(r,0,sizeof *r); r->status=200; r->erro=REDE_OK;
  assert(!q->seguir && !q->tls_pessoal);
  assert(!strchr(q->url,'?') && !strstr(q->url,"synthetic-password"));
  if(strstr(q->url,"/v1/aiopair/")) {
    char op[24]; js_texto_raiz(q->corpo,"op",op,sizeof op);
    assert(!strstr(q->corpo,"synthetic-password"));
    if(!strcmp(op,"create")) {
      if(mode==CANCEL_CREATE) aiopair_close();
      r->corpo=strdup(mode==CREATE_HTML?"<html>missing route</html>":"{\"state\":\"waiting\"}");
    } else if(!strcmp(op,"take")) {
      if(mode==POLL_NETWORK) { r->erro=REDE_PRAZO; return 0; }
      if(mode==POLL_SERVER) { r->status=503; return 0; }
      if(mode==RETRY_TAKE && !takes++) { r->erro=REDE_PRAZO; return 0; }
      if(mode==EXPIRED || (mode!=RETRY_TAKE && takes++)) { r->status=410; return 0; }
      r->corpo=strdup(mode==WRONG_ORIGIN?
        "{\"origin\":\"https://other.invalid\",\"uuid\":\"sample-uuid\",\"password\":\"synthetic-password\"}":
        "{\"origin\":\"https://instance.invalid\",\"uuid\":\"sample-uuid\",\"password\":\"synthetic-password\"}");
    } else if(!strcmp(op,"result")) {
      results++; js_texto_raiz(q->corpo,"state",sentResult,sizeof sentResult);
      if(mode==CONFIRM_SERVER || (mode==RETRY_CONFIRM && results==1)) { r->status=503; return 0; }
      char response[64]; snprintf(response,sizeof response,"{\"state\":\"%s\"}",sentResult);
      r->corpo=strdup(response);
    } else {
      assert(!strcmp(op,"close")); closes++; r->corpo=strdup("{\"state\":\"closed\"}");
    }
  } else {
    instanceCalls++; assert(!strcmp(q->url,"https://instance.invalid/api/v1/user"));
    assert(q->cabecalhos && !strncmp(q->cabecalhos[0],"Authorization: Basic ",21));
    if(mode==CANCEL_VALIDATE) aiopair_close();
    if(mode==BAD_AUTH) r->status=401;
    if(mode==REDIRECT) r->status=302;
    if(mode==INSTANCE_NETWORK) r->erro=REDE_PRAZO;
    if(mode==INSTANCE_SERVER) r->status=503;
    r->corpo=strdup(mode==BAD_RESPONSE?"{\"success\":false}":
      "{\"success\":true,\"data\":{\"userData\":{\"presets\":[{\"type\":\"Torrentio\"}]}}}");
  }
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); memset(r,0,sizeof *r); }
int main(void) {
  for(mode=0;mode<CASES;mode++) {
    aiopair_close(); takes=instanceCalls=results=closes=0; sentResult[0]=0;
    Pair *p=calloc(1,sizeof *p); p->generation=generation;
    strcpy(p->origin,"https://instance.invalid");
    assert(randomKey(p->id) && randomKey(p->tv) && randomKey(p->phone));
    assert(strcmp(p->tv,p->phone) && strcmp(p->id,p->phone));
    run(p);
    int success=mode==GOOD || mode==RETRY_CONFIRM || mode==RETRY_TAKE;
    char auth[2048],origin[256],url[512];
    int final=aiopair_state(url,sizeof url);
    assert(aiopair_take(auth,sizeof auth,origin,sizeof origin)==success);
    if(success) { assert(final==AIOPAIR_SYNCED); assert(!strcmp(origin,"https://instance.invalid")); assert(!strcmp(sentResult,"synced")); }
    if(mode==POLL_NETWORK || mode==POLL_SERVER || mode==CREATE_HTML || mode==CONFIRM_SERVER)
      assert(final==AIOPAIR_UNAVAILABLE && !url[0]);
    if(mode==EXPIRED) assert(final==AIOPAIR_EXPIRED && !url[0]);
    if(mode==CANCEL_CREATE || mode==CANCEL_VALIDATE) assert(final==AIOPAIR_OFF && !url[0] && closes==1);
    if(mode==CANCEL_CREATE || mode==CREATE_HTML || mode==EXPIRED || mode==POLL_NETWORK || mode==POLL_SERVER)
      assert(!instanceCalls && !results);
    if(mode==WRONG_ORIGIN) assert(!instanceCalls && !strcmp(sentResult,"error"));
    if(mode==RETRY_CONFIRM) assert(results==2);
    assert(!aiopair_take(auth,sizeof auth,origin,sizeof origin));
    memset(auth,0,sizeof auth);
  }
  aiopair_close(); assert(state==AIOPAIR_OFF && !savedAuth[0] && !savedOrigin[0] && !pairUrl[0]);
  assert(!aiopair_start("http://instance.invalid"));
  char tooLong[160]; memset(tooLong,'a',sizeof tooLong-1); tooLong[sizeof tooLong-1]=0;
  memcpy(tooLong,"https://",8); fixtureService=tooLong;
  assert(!aiopair_start("https://instance.invalid") && state==AIOPAIR_UNAVAILABLE && !pairUrl[0]);
  puts("aiopair TV: receipt/confirmation, auth, origin, expiry, cancellation, network/server and malformed responses passed");
}
