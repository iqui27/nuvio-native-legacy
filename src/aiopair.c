// HTTPS QR pairing, using the app worker and its short-lived session pattern.
// Only POST bodies carry credentials. No persistence, response/body/URL logging,
// redirects, personal TLS bypass, or authorization outside the pinned origin.
#include "aiopair.h"
#include "aiostreams.h"
#include "aionotice.h"
#include "rede.h"
#include "js.h"
#include "jsw.h"
#include "qr.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include "nvbackend.h"
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
static pthread_mutex_t pairLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned generation;
static int state, ready;
static char pairUrl[512], savedAuth[2048], savedOrigin[256];
typedef struct { unsigned generation; char origin[256], id[40], tv[40], phone[40]; } Pair;
static int active(Pair *p) {
  pthread_mutex_lock(&pairLock); int ok=p->generation==generation; pthread_mutex_unlock(&pairLock); return ok;
}
static void publish(Pair *p,int next) {
  pthread_mutex_lock(&pairLock);
  if(p->generation==generation) {
    state=next;
    if(next==AIOPAIR_EXPIRED || next==AIOPAIR_UNAVAILABLE || next==AIOPAIR_SYNCED) memset(pairUrl,0,sizeof pairUrl);
  }
  pthread_mutex_unlock(&pairLock);
}
static char *request(Pair *p,const char *op,const char *result,const char *reason,int *status) {
  char url[768]; snprintf(url,sizeof url,"%s/v1/aiopair/%s",NV_REC_URL,p->id);
  Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w); jsw_cs(&w,"op",op);
  if(!strcmp(op,"create")) {
    jsw_cs(&w,"origin",p->origin); jsw_cs(&w,"tv",p->tv); jsw_cs(&w,"phone",p->phone);
  } else jsw_cs(&w,"key",p->tv);
  if(result) jsw_cs(&w,"state",result);
  if(reason) jsw_cs(&w,"reason",reason);
  jsw_obj_fim(&w); const char *body=jsw_texto_final(&w);
  const char *headers[]={"Content-Type: application/json","Cache-Control: no-store",NULL};
  RedePedido q={.url=url,.metodo="POST",.cabecalhos=headers,.corpo=body,.n_corpo=body?strlen(body):0,
    .prazo_ms=5000,.max_bytes=8192,.seguir=0};
  RedeResposta r={0}; rede_pedir(&q,&r); *status=r.erro==REDE_OK?r.status:0;
  printf("[aiopair] op=%s status=%d transport=%d\n",op,r.status,r.erro);
  char *out=NULL;
  if(r.erro==REDE_OK && r.status==200) { out=r.corpo; r.corpo=NULL; }
  rede_resposta_limpar(&r); jsw_livre(&w); return out;
}
static int randomKey(char out[40]) {
  unsigned char bytes[16]; FILE *f=fopen("/dev/urandom","rb");
  if(!f) return 0;
  size_t n=fread(bytes,1,sizeof bytes,f); fclose(f); if(n!=sizeof bytes) return 0;
  snprintf(out,40,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
  memset(bytes,0,sizeof bytes); return 1;
}
static void wipe(char *s) { if(s) { memset(s,0,strlen(s)); free(s); } }
static int responseState(const char *body,const char *expected) {
  char actual[32]={0};
  return body && aio_json_valid(body) && js_texto_raiz(body,"state",actual,sizeof actual) && !strcmp(actual,expected);
}
static void *run(void *arg) {
  Pair *p=arg; int status=0;
  char *body=request(p,"create",NULL,NULL,&status);
  int created=responseState(body,"waiting"); wipe(body);
  if(!created) { publish(p,AIOPAIR_UNAVAILABLE); goto done; }
  if(!active(p)) goto close;
  pthread_mutex_lock(&pairLock);
  if(p->generation==generation) {
    snprintf(pairUrl,sizeof pairUrl,"%s/aio/#%s",NV_REC_URL,p->phone); state=AIOPAIR_WAITING;
  }
  pthread_mutex_unlock(&pairLock);
  time_t until=time(NULL)+300;
  int failedPolls=0;
  while(active(p) && time(NULL)<until) {
    body=request(p,"take",NULL,NULL,&status);
    if(status==410) { wipe(body); publish(p,AIOPAIR_EXPIRED); break; }
    if(!body || !aio_json_valid(body)) {
      wipe(body);
      if((!status || status==429 || status>=500) && ++failedPolls<3) {
        for(int i=0;i<15 && active(p);i++) usleep(100000);
        continue;
      }
      publish(p,AIOPAIR_UNAVAILABLE); break;
    }
    failedPolls=0;
    if(body) {
      char origin[256]={0},uuid[128]={0},password[769]={0},auth[2048]={0},credentials[1024]={0};
      js_texto_raiz(body,"origin",origin,sizeof origin); js_texto_raiz(body,"uuid",uuid,sizeof uuid);
      js_texto_raiz(body,"password",password,sizeof password); wipe(body);
      if(uuid[0] && active(p)) {
        publish(p,AIOPAIR_CHECKING);
        const char *reason=!strcmp(origin,p->origin)?"auth":"origin";
        int valid=!strcmp(origin,p->origin) && strspn(uuid,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==strlen(uuid) && password[0];
        snprintf(credentials,sizeof credentials,"%s:%s",uuid,password);
        valid=valid && aio_basic(credentials,auth,sizeof auth);
        memset(credentials,0,sizeof credentials); memset(uuid,0,sizeof uuid); memset(password,0,sizeof password);
        if(valid) {
          char url[320]; snprintf(url,sizeof url,"%s/api/v1/user",p->origin);
          const char *headers[]={auth,"Cache-Control: no-store",NULL};
          RedePedido q={.url=url,.cabecalhos=headers,.prazo_ms=12000,.max_bytes=1024*1024,.seguir=0};
          RedeResposta r={0}; rede_pedir(&q,&r);
          AioSnapshot *snapshot=calloc(1,sizeof *snapshot);
          if(snapshot && r.erro==REDE_OK && r.status==200 && r.corpo && aio_json_valid(r.corpo)) aio_configuracao(snapshot,r.corpo);
          valid=snapshot && snapshot->configurada;
          reason=r.erro!=REDE_OK?"network":r.status==429 || r.status>=500?"server":
            r.status==401 || r.status==403?"auth":"response";
          if(snapshot) { memset(snapshot,0,sizeof *snapshot); free(snapshot); }
          if(r.corpo) memset(r.corpo,0,strlen(r.corpo)); rede_resposta_limpar(&r);
        }
        if(active(p)) {
          int confirmed=0;
          for(int attempt=0;attempt<3 && active(p);attempt++) {
            body=request(p,"result",valid?"synced":"error",valid?NULL:reason,&status);
            confirmed=responseState(body,valid?"synced":"error"); wipe(body);
            if(confirmed || (status>=400 && status<500)) break;
            usleep(100000);
          }
          if(!confirmed) { memset(auth,0,sizeof auth); publish(p,AIOPAIR_UNAVAILABLE); goto close; }
          pthread_mutex_lock(&pairLock);
          if(p->generation==generation) {
            state=valid?AIOPAIR_SYNCED:!strcmp(reason,"network") || !strcmp(reason,"server")?AIOPAIR_UNAVAILABLE:AIOPAIR_ERROR;
            if(state!=AIOPAIR_ERROR || !strcmp(reason,"origin")) memset(pairUrl,0,sizeof pairUrl);
            if(valid) { snprintf(savedAuth,sizeof savedAuth,"%s",auth); snprintf(savedOrigin,sizeof savedOrigin,"%s",p->origin); ready=1; }
          }
          pthread_mutex_unlock(&pairLock);
        }
        memset(auth,0,sizeof auth);
        if(active(p) && valid) goto done;
        if(active(p) && (!strcmp(reason,"origin") || !strcmp(reason,"network") || !strcmp(reason,"server"))) goto close;
      }
      memset(origin,0,sizeof origin); memset(uuid,0,sizeof uuid); memset(password,0,sizeof password);
    }
    for(int i=0;i<15 && active(p);i++) usleep(100000);
  }
  if(active(p) && time(NULL)>=until) publish(p,AIOPAIR_EXPIRED);
close:
  body=request(p,"close",NULL,NULL,&status); wipe(body);
done:
  memset(p,0,sizeof *p); free(p); return NULL;
}
void aiopair_close(void) {
  pthread_mutex_lock(&pairLock); generation++; state=AIOPAIR_OFF; ready=0;
  memset(savedAuth,0,sizeof savedAuth); memset(savedOrigin,0,sizeof savedOrigin); memset(pairUrl,0,sizeof pairUrl);
  pthread_mutex_unlock(&pairLock);
}
int aiopair_start(const char *origin) {
  aiopair_close(); char parsed[256];
  if(strncmp(NV_REC_URL,"https://",8) || strlen(NV_REC_URL)+sizeof "/aio/#"-1+32>QR_MAX_BYTES ||
     !aio_url(origin,parsed,sizeof parsed) || strcmp(origin,parsed) || strncmp(origin,"https://",8)) {
    pthread_mutex_lock(&pairLock); state=AIOPAIR_UNAVAILABLE; pthread_mutex_unlock(&pairLock); return 0;
  }
  pthread_mutex_lock(&pairLock); state=AIOPAIR_UNAVAILABLE; pthread_mutex_unlock(&pairLock);
  Pair *p=calloc(1,sizeof *p); if(!p) return 0;
  snprintf(p->origin,sizeof p->origin,"%s",origin);
  char random[40];
  if(!randomKey(p->tv) || !randomKey(random)) { memset(p,0,sizeof *p); free(p); return 0; }
  int at=0; for(int i=0;random[i];i++) if(random[i]!='-') p->phone[at++]=random[i];
  memcpy(p->id,p->phone,8); p->id[8]=0; memset(random,0,sizeof random);
  pthread_mutex_lock(&pairLock); p->generation=generation; state=AIOPAIR_STARTING; pthread_mutex_unlock(&pairLock);
  pthread_t t; if(pthread_create(&t,NULL,run,p)) { publish(p,AIOPAIR_UNAVAILABLE); free(p); return 0; }
  pthread_detach(t); return 1;
}
int aiopair_state(char *url,size_t n) {
  pthread_mutex_lock(&pairLock); int s=state; if(url && n) snprintf(url,n,"%s",pairUrl); pthread_mutex_unlock(&pairLock); return s;
}
int aiopair_take(char *auth,size_t n,char *origin,size_t no) {
  pthread_mutex_lock(&pairLock); int r=ready;
  if(r) { snprintf(auth,n,"%s",savedAuth); snprintf(origin,no,"%s",savedOrigin); memset(savedAuth,0,sizeof savedAuth); ready=0; }
  pthread_mutex_unlock(&pairLock); return r;
}
