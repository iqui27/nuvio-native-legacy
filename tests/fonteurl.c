#define FONTEURL_TEST 1
#include "../src/fonteurl.c"
#include <assert.h>
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}
typedef struct {int status;const char *final;_Atomic int entrou,liberar,acabou;int esperar;} Mock;
static _Atomic(Mock *) mock;
static int pedir(const RedePedido *p,RedeResposta *r) {
  Mock *m=atomic_load(&mock);
  assert(!strcmp(p->metodo,"GET") && p->prazo_ms==30000 && p->max_bytes==LEGFONTE_MAX_BYTES && p->seguir && !p->tls_pessoal && p->job);
  atomic_store(&m->entrou,1);
  if(m->esperar)while(!atomic_load(&m->liberar))SDL_Delay(1);
  r->status=m->status;r->n_corpo=16;r->corpo=calloc(17,1);assert(r->corpo);memcpy(r->corpo,"OTTOtest",8);
  snprintf(r->final,sizeof r->final,"%s",m->final?m->final:p->url);
  atomic_store(&m->acabou,1);return 1;
}
static void esperar(_Atomic int *flag) {for(int i=0;i<3000 && !atomic_load(flag);i++)SDL_Delay(1);assert(atomic_load(flag));}
static void resultado(char out[80]) {
  for(int i=0;i<3000;i++){if(fonteurl_resultado(out,80))return;SDL_Delay(1);}assert(0);
}
int main(int argc,char **argv) {
  assert(argc==2);testePedir=pedir;
  assert(!fonteurl_iniciar("http://fonts.example/a.ttf",argv[1]));
  assert(!fonteurl_iniciar("https://user:pass@fonts.example/a.ttf",argv[1]));
  assert(!fonteurl_iniciar("https:///a.ttf",argv[1]));
  Mock good={.status=200};atomic_store(&mock,&good);
  assert(fonteurl_iniciar("https://fonts.example/a.ttf",argv[1]));
  assert(!fonteurl_iniciar("https://fonts.example/other.ttf",argv[1]));
  char event[80],path[768];resultado(event);assert(!strncmp(event,"R:entrada-",10));
  snprintf(path,sizeof path,"%s/%s",argv[1],event+2);struct stat st;assert(!stat(path,&st) && st.st_size==16 && (st.st_mode&0777)==0600);unlink(path);
  Mock dead={.status=404};atomic_store(&mock,&dead);
  assert(fonteurl_iniciar("https://fonts.example/missing.ttf",argv[1]));resultado(event);assert(!strcmp(event,"E"));
  Mock downgrade={.status=200,.final="http://fonts.example/insecure.ttf"};atomic_store(&mock,&downgrade);
  assert(fonteurl_iniciar("https://fonts.example/redirect.ttf",argv[1]));resultado(event);assert(!strcmp(event,"E"));
  Mock slow={.status=200,.esperar=1};atomic_store(&mock,&slow);
  assert(fonteurl_iniciar("https://fonts.example/slow.ttf",argv[1]));esperar(&slow.entrou);
  fonteurl_cancelar();atomic_store(&mock,&good);
  assert(fonteurl_iniciar("https://fonts.example/new.ttf",argv[1]));
  atomic_store(&slow.liberar,1);esperar(&slow.acabou);resultado(event);assert(!strncmp(event,"R:",2));
  snprintf(path,sizeof path,"%s/%s",argv[1],event+2);assert(!stat(path,&st));unlink(path);
  fonteurl_cancelar();
  puts("PASS: bounded asynchronous HTTPS font downloads, private staging, failed link, downgrade rejection and cancellation/new-request isolation");
}
