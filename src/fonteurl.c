#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "fonteurl.h"
#include "legfontes.h"
#include "rede.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct {
  _Atomic int refs,pronto;
  RedeJob *job;
  char url[4096],pasta[640],arquivo[64],caminho[768];
  int ok;
} Download;
static Download *atual;
static unsigned sequencia;
#ifdef FONTEURL_TEST
static int (*testePedir)(const RedePedido *,RedeResposta *);
#endif
static void soltar(Download *d) {
  if(!d || atomic_fetch_sub_explicit(&d->refs,1,memory_order_acq_rel)!=1)return;
  if(d->caminho[0]){dados_fs_travar();unlink(d->caminho);dados_fs_liberar();}
  rede_job_soltar(d->job);free(d);
}
void fonteurl_cancelar(void) {
  Download *d=atual;atual=NULL;
  if(d){rede_job_cancelar(d->job);soltar(d);}
}
static int https(const char *url) {
  if(!url || strncmp(url,"https://",8) || !url[8] || url[8]=='/' || strlen(url)>=4096)return 0;
  for(const unsigned char *p=(const unsigned char *)url;*p;p++)if(*p<=32 || *p==127 || *p=='\\')return 0;
  const char *end=strpbrk(url+8,"/?#");if(!end)end=url+strlen(url);
  return end>url+8 && !memchr(url+8,'@',(size_t)(end-url-8));
}
static void *executar(void *arg) {
  Download *d=arg;RedeResposta r={0};
  RedePedido p={.metodo="GET",.url=d->url,.prazo_ms=30000,.max_bytes=LEGFONTE_MAX_BYTES,
    .max_cabecalhos=16384,.seguir=1,.job=d->job};
#ifdef FONTEURL_TEST
  int recebeu=testePedir?testePedir(&p,&r):rede_pedir(&p,&r);
#else
  int recebeu=rede_pedir(&p,&r);
#endif
  if(recebeu && r.erro==REDE_OK && r.status==200 && r.corpo && r.n_corpo>=12 &&
     r.n_corpo<=LEGFONTE_MAX_BYTES && https(r.final[0]?r.final:d->url) && rede_job_estado(d->job)==REDE_OK) {
    // No user-supplied filename/path, and no overwrite of an existing file.
    snprintf(d->caminho,sizeof d->caminho,"%s/%s",d->pasta,d->arquivo);
    dados_fs_travar();
    int fd=open(d->caminho,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    size_t pos=0;int ok=fd>=0;
    while(ok && pos<r.n_corpo) {
      ssize_t n=write(fd,r.corpo+pos,r.n_corpo-pos);
      if(n<0 && errno==EINTR)continue;
      if(n<=0)ok=0;else pos+=(size_t)n;
    }
    if(fd>=0 && close(fd))ok=0;
    if(!ok) {
      // A failed exclusive open does not own the conflicting file.
      if(fd>=0)unlink(d->caminho);
      d->caminho[0]=0;
    }
    dados_fs_liberar();d->ok=ok;
  }
  rede_resposta_limpar(&r);
  atomic_store_explicit(&d->pronto,1,memory_order_release);soltar(d);return NULL;
}
int fonteurl_iniciar(const char *url,const char *pasta) {
  if(atual || !https(url) || !pasta || !*pasta || strlen(pasta)>=640)return 0;
  Download *d=calloc(1,sizeof *d);if(!d)return 0;
  atomic_init(&d->refs,2);atomic_init(&d->pronto,0);
  RedeGrupo *g=rede_grupo_criar();d->job=g?rede_job_criar(g):NULL;rede_grupo_soltar(g);
  if(!d->job){free(d);return 0;}
  snprintf(d->url,sizeof d->url,"%s",url);snprintf(d->pasta,sizeof d->pasta,"%s",pasta);
  snprintf(d->arquivo,sizeof d->arquivo,"entrada-%016llx.tmp",(unsigned long long)((SDL_GetTicks64()<<24) | (++sequencia & 0xffffffu)));
  pthread_t thread;
  if(pthread_create(&thread,NULL,executar,d)){soltar(d);soltar(d);return 0;}
  pthread_detach(thread);atual=d;return 1;
}
int fonteurl_resultado(char *evento,size_t n) {
  Download *d=atual;
  if(!evento || n<80 || !d || !atomic_load_explicit(&d->pronto,memory_order_acquire))return 0;
  atual=NULL;
  int ok=d->ok && rede_job_estado(d->job)==REDE_OK;
  snprintf(evento,n,ok?"R:%s":"E",d->arquivo);
  if(ok)d->caminho[0]=0; // The registry now owns the completed staging file.
  soltar(d);return 1;
}
