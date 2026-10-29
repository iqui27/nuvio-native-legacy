/* Deterministic elapsed time and curl doubles; no external requests or secrets. */
#include <time.h>
static unsigned long elapsed;
static int test_clock(clockid_t id, struct timespec *ts) {
  (void)id; ts->tv_sec=elapsed/1000; ts->tv_nsec=(elapsed%1000)*1000000; return 0;
}
#define clock_gettime test_clock
#ifndef TRANSPORT_UNDER_TEST
#define TRANSPORT_UNDER_TEST "../src/rede.c"
#endif
#include TRANSPORT_UNDER_TEST
#undef clock_gettime
#include <assert.h>
#include <stdarg.h>
typedef struct { long timeout; size_t (*write)(void *,size_t,size_t,void *); void *data; } Fake;
static int mode,calls,created,failed;
static void *init(void){created++;return calloc(1,sizeof(Fake));}
static int option(void *c,int op,...){
  Fake *f=c;va_list ap;va_start(ap,op);
  if(op==OPT_TIMEOUT_MS)f->timeout=va_arg(ap,long);
  if(op==OPT_WRITEFUNCTION)f->write=va_arg(ap,size_t (*)(void *,size_t,size_t,void *));
  if(op==OPT_WRITEDATA)f->data=va_arg(ap,void *);
  va_end(ap);return 0;
}
static int perform(void *c){
  Fake *f=c;calls++;
  if(mode==1&&calls==1){elapsed+=f->timeout;failed=1;return 28;}
  failed=0;
  for(int i=0;i<18;i++){
    elapsed+=1000;
    if(f->timeout&&i*1000>=f->timeout){failed=1;return 28;}
    f->write(" ",1,1,f->data);
  }
  f->write("[]",1,2,f->data);return 0;
}
static int info(void *c,int op,...){
  (void)c;va_list ap;va_start(ap,op);
  if(op==INFO_NUM_CONNECTS)*va_arg(ap,long *)=mode==1&&calls==1?0:1;
  if(op==INFO_RESPONSE_CODE)*va_arg(ap,long *)=failed?0:200;
  va_end(ap);return 0;
}
int main(int argc,char **argv){
  (void)argv;assert(argc==1);pronto=1;curl_init=init;curl_cleanup=free;curl_setopt_f=option;curl_perform=perform;curl_getinfo=info;
  for(mode=0;mode<2;mode++){
    calls=created=0;int status=0;
    char *b=rede_baixar_st("http://127.0.0.1/player_api.php?action=get_vod_streams",15,NULL,&status);
    assert(b&&status==200&&strstr(b,"[]"));free(b);
    assert(calls==(mode?2:1));if(mode)assert(created==2);
  }
  puts("Xtream streaming list and exhausted reused connection: PASS");return 0;
}
